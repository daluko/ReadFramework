/*******************************************************************************************************
ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ.

Copyright (C) 2016 Markus Diem <diem@caa.tuwien.ac.at>
Copyright (C) 2016 Stefan Fiel <fiel@caa.tuwien.ac.at>
Copyright (C) 2016 Florian Kleber <kleber@caa.tuwien.ac.at>

This file is part of ReadFramework.

ReadFramework is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ReadFramework is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

The READ project  has  received  funding  from  the European  Union’s  Horizon  2020
research  and innovation programme under grant agreement No 674943

related links:
[1] http://www.caa.tuwien.ac.at/cvl/
[2] https://transkribus.eu/Transkribus/
[3] https://github.com/TUWien/
[4] http://nomacs.org
*******************************************************************************************************/

#include "Image.h"
#include "Utils.h"
#include "Elements.h"
#include "ElementsHelper.h"
#include "WhiteSpaceAnalysis.h"
#include "FontStyleClassification.h"
#include "ImageProcessor.h"
#include "PageParser.h"
#include "DebugDavid.h"

#pragma warning(push, 0)	// no warnings from includes
#include "opencv2/imgproc.hpp"
#include <QDir>
#include <QFileInfo>
#include <QPainter>
#include <QJsonObject>		// needed for LabelInfo
#include <QJsonDocument>	// needed for LabelInfo
#include <QJsonArray>		// needed for LabelInfo
#pragma warning(pop)

namespace rdf {
	// TextPatch --------------------------------------------------------------------
	TextPatch::TextPatch() {
		mTextPatch = cv::Mat();
		mLabel->setTrueLabel(LabelInfo(1, "unknown_font"));
	}

	TextPatch::TextPatch(cv::Mat tpImg, int fixedPatchSize, int textureSize, const QString & id) : BaseElement(id) {

		mTextPatch = tpImg.clone();
		
		if (textureSize > 0)
			setTextureSize(textureSize);	

		if (fixedPatchSize != -1 && fixedPatchSize < 1) {
			qDebug() << "Patch size parameter invalid. Patch size won't be modified.";
			fixedPatchSize = -1;
		}		

		if (fixedPatchSize != -1)
			adaptPatchHeight(fixedPatchSize);

		if (!generatePatchTexture())
			qWarning() << "Failed to generate texture from text patch.";

		mLabel->setTrueLabel(LabelInfo(1, "unknown_font"));
	}

	TextPatch::TextPatch(cv::Mat textImg, Rect tpRect, int fixedPatchSize, int textureSize,
		const QString & id) : BaseElement(id) {

		mTextPatch = textImg.clone();
		
		if (textureSize > 0)
			setTextureSize(textureSize);

		if (fixedPatchSize!=-1)
			adaptPatchHeight(textImg, tpRect, fixedPatchSize);

		if (!generatePatchTexture())
			qWarning() << "Failed to generate texture from text patch.";

		mLabel->setTrueLabel(LabelInfo(1, "unknown_font"));
	}

	TextPatch::TextPatch(QString text, const LabelInfo label, int fixedPatchSize,
		int textureSize, bool cropText ,const QString & id) : BaseElement(id){

		QFont font = FontStyleClassification::labelNameToFont(label.name());
		mLabel->setTrueLabel(label);
		generateTextImage(text, font, cropText);
		
		cv::Mat debug = mTextPatch.clone();
		if (textureSize > 0)
			setTextureSize(textureSize);

		if(fixedPatchSize != -1)
			adaptPatchHeight(fixedPatchSize);

		if(!generatePatchTexture())
			qWarning() << "Failed to generate texture from text patch.";
	}

	bool TextPatch::isEmpty() const {
		if(mTextPatch.empty() || mPatchTexture.empty())
			return true;
		else
			return false;
	}

	int TextPatch::textureSize() const {
		return mTextureSize;
	}

	void TextPatch::setTextureSize(int textureSize) {
		mTextureSize = textureSize;
		mMaxPatchHeight = floor(textureSize / mMinLineNumber);
	}

	bool TextPatch::generatePatchTexture() {

		if (mTextPatch.empty()) {
			qWarning() << "Couldn't create patch texture! Text patch is empty";
			mPatchTexture = cv::Mat();
			return false;
		}
		
		cv::Mat textPatch = mTextPatch;

		//rescaling text patch image to fit at least mMinLineNumber lines of text patches into texture image
		if (mTextPatch.rows > mMaxPatchHeight) {
			
			double sf = mMaxPatchHeight / mTextPatch.rows;

			if (sf*mTextPatch.size().height < 1 || sf * mTextPatch.size().width < 1) {
				mPatchTexture = cv::Mat();
				qWarning() << "Couldn't create patch texture! Scaling text patch results in size < 1 pixel.";
				return false;
			}
			
			resize(mTextPatch, textPatch, cv::Size(), sf, sf, cv::INTER_LINEAR); //TODO test different interpolation modes
			//cv::Mat textPatch1, textPatch2;
			//resize(mTextPatch, textPatch1, cv::Size(), sf, sf, cv::INTER_CUBIC);
			//resize(mTextPatch, textPatch2, cv::Size(), sf, sf, cv::INTER_LANCZOS4);
		}
		
		cv::Mat texture = textPatch;

		//get text input image and replicate it to fill an image patch of size mTextureSize
		double lineNum = floor((double)mTextureSize / (double)texture.rows);

		while (texture.cols < mTextureSize*lineNum) {
			cv::hconcat(texture, textPatch, texture);
		}

		int i = 0;
		mPatchTexture = cv::Mat();
		int pad = (int) floor((mTextureSize - (texture.rows*lineNum)) / lineNum);
		int hPad = (int)floor((double)pad / 2.0);
		int tPad = (pad % 2 == 0) ? hPad : hPad + 1;

		while (i < lineNum) {
			int start = i * mTextureSize;
			int end = (i + 1) * mTextureSize;
			cv::Mat textureLine = texture(cv::Range::all(), cv::Range(start, end));

			//add blank border to text lines
			copyMakeBorder(textureLine, textureLine, hPad, tPad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(255, 255, 255, 255));

			if (mPatchTexture.empty())
				mPatchTexture = textureLine.clone();
			else
				cv::vconcat(mPatchTexture, textureLine, mPatchTexture);
			i++;
		}

		//pad bottom of texture with white border
		copyMakeBorder(mPatchTexture, mPatchTexture, 0, mTextureSize- mPatchTexture.rows, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(255,255,255,255));

		return true;
	}

	bool TextPatch::generateTextImage(QString text, QFont font, bool cropImg) {

		if (text.isEmpty()) {
			qWarning() << "Could not generate text image. No input text found.";
			mTextPatch = cv::Mat();
			return false;
		}

		QImage qImg(1, 1, QImage::Format_ARGB32);	//used only for estimating bb size
		QPainter painter(&qImg);
		painter.setFont(font);
		QRect tbb = painter.boundingRect(QRect(0, 0, 1, 1), Qt::AlignTop | Qt::AlignLeft, text);
		painter.end();

		//reset painter device to optimized size
		qImg = QImage(tbb.size(), QImage::Format_ARGB32);
		qImg.fill(QColor(255, 255, 255, 255)); //white background
		painter.begin(&qImg);

		painter.setFont(font);
		painter.drawText(tbb, Qt::AlignTop | Qt::AlignLeft, text);
		mTextPatch = Image::qImage2Mat(qImg);

		//crop image - ignoring white border regions
		if (cropImg) {
			cv::Mat img_gray(mTextPatch.size(), CV_8UC1);
			cv::cvtColor(mTextPatch, img_gray, CV_BGR2GRAY);

			cv::Mat points;
			cv::findNonZero(img_gray == 0, points);
			mTextPatch = mTextPatch(cv::boundingRect(points));
		}

		return true;
	}

	void TextPatch::adaptPatchHeight(int patchHeight){
		adaptPatchHeight(mTextPatch, Rect(mTextPatch), patchHeight);
	}

	void TextPatch::adaptPatchHeight(cv::Mat img, Rect tpRect, int patchHeight){

		//adapt text patch height according to patchHeight parameter
		cv::Mat textPatch;
		Rect imgRect = Rect(img);

		int padding = patchHeight - (int)tpRect.height();
		if (tpRect.height() < patchHeight) {

			if (padding >= 10) 	//enlarge boundary of text patch (avoid text pixels at boundary being replicated)
				padding = 10;

			int hPad = (int)floor((double)padding / 2.0);
			int tPad = (padding % 2 == 0) ? hPad : hPad + 1;

			tpRect.move(Vector2D(0, -tPad));
			tpRect.setSize(tpRect.size() + Vector2D(0, hPad + tPad));

			if (!imgRect.contains(tpRect)) //ensure crop rect is within image boundaries
				tpRect = imgRect.intersected(tpRect);

			textPatch = img(tpRect.toCvRect());
			padding = patchHeight - textPatch.size().height; //update padding size

			//TODO consider filling with white /average background color
			if (padding > 0) { //replicate border to fill patch area
				hPad = (int)floor((double)padding / 2.0);
				tPad = (padding % 2 == 0) ? hPad : hPad + 1;

				if(tpRect==imgRect)
					copyMakeBorder(textPatch, textPatch, tPad, hPad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(255,255,255,255));
				else
					copyMakeBorder(textPatch, textPatch, tPad, hPad, 0, 0, cv::BORDER_REPLICATE);
			}
		}
		else if (tpRect.height() > patchHeight) {

			cv::Mat vPP;
			textPatch = img(tpRect.toCvRect());
			vPP = IP::grayscale(textPatch);
			reduce(vPP, vPP, 1, cv::REDUCE_SUM, CV_32F);

			while (textPatch.rows > patchHeight) {

				bool deleteFirstRow = vPP.at<float>(0, 0) > vPP.at<float>(vPP.rows - 1, 0);

				int f = (deleteFirstRow) ? 1 : 0;
				int l = (deleteFirstRow) ? textPatch.rows : textPatch.rows - 1;

				textPatch = textPatch(cv::Range(f, l), cv::Range::all());
				vPP = vPP(cv::Range(f, l), cv::Range::all()).clone();
			}
		}
		else {
			textPatch = img(tpRect.toCvRect());
		}

		mTextPatch = textPatch.clone();
	}

	QSharedPointer<PixelLabel> TextPatch::label() const {
		return mLabel;
	}

	cv::Mat TextPatch::patchTexture() const {
		return mPatchTexture;
	}

	cv::Mat TextPatch::textPatchImg() const{
		return mTextPatch;
	}

	int TextPatch::maxPatchHeight() const{
		return mMaxPatchHeight;
	}

	void TextPatch::setPolygon(const Polygon & polygon) {
		mPoly = polygon;
	}

	Polygon TextPatch::polygon() const {
		return mPoly;
	}

	//FontDataGenerator --------------------------------------------------------------------
	int FontDataGenerator::computePatchSizeEstimate(QString dataSetDir) {

		int patchSize = -1;

		QDir dir(dataSetDir);
		if (!dir.exists()) {
			qWarning() << "Failed to compute patch size estimate. Directory does not exist: " << dataSetDir;
			return patchSize;
		}
		
		qInfo() << "Computing patch size estimate for files in directory: " << dataSetDir;

		QStringList filters;
		filters << "*.tif" << "*.jpg";
		QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
		
		QVector<QSharedPointer<TextLine>> wordRegions = QVector<QSharedPointer<TextLine>>();
		QVector<QSharedPointer<TextLine>> lineRegions = QVector<QSharedPointer<TextLine>>();

		for (auto f : fileInfoList) {

			QString imagePath = f.absoluteFilePath();
			QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(imagePath);

			if (!QFileInfo(xmlPath).exists())
				continue;

			//TODO consider using text lines regions instead or check dynamically for both region types
			//wordRegions = loadRegions<TextLine>(imagePath, Region::type_word);
			wordRegions = loadRegions<TextLine>(imagePath, Region::type_word);
			lineRegions = loadRegions<TextLine>(imagePath, Region::type_text_line);
		}

		if (wordRegions.isEmpty()) {
			qCritical() << "Failed to compute patch size estimate! No word regions found!";
			return patchSize;
		}

		QList<int> heights;
		for (auto wr : wordRegions) {
			Rect wrr = Rect::fromPoints(wr->polygon().toPoints());
			int wrh = qRound(wrr.height());
			heights << wrh;
		}

		patchSize = qRound(Algorithms::statMoment(heights, 0.95));

		return patchSize;
	}

	int FontDataGenerator::computePatchSizeEstimate(QStringList samples, QVector<QFont> fonts){

		if (fonts.empty() || samples.empty()) {
			qCritical() << "Could not compute patch size estimate. Make sure to correctely pass font information and text samples.";
			qWarning() << "No fixed patch size will be used! Height of text patch will be unmodified.";
			return -1;
		}

		LabelManager labelManager = FontDataGenerator::generateFontLabelManager(fonts);
		auto patches = FontDataGenerator::generateTextPatches(samples, labelManager);

		if (fonts.empty() || samples.empty()) {
			qCritical() << "Failed to compute text patches for patch size estimation!";
			qWarning() << "No fixed patch size will be used! Height of text patch will be unmodified.";
			return -1;
		}

		QList<int> heights;
		for (auto p : patches)
			heights << p->textPatchImg().rows;

		int patchHeightEstimate = qRound(Algorithms::statMoment(heights, 0.95));
		qInfo() << "Patch height estimate for synthetic data set = " << QString::number(patchHeightEstimate);

		return patchHeightEstimate;
	}

	QVector<QFont> FontDataGenerator::generateFontStyles(){

		QVector<QFont> fontStyleManager;

		QFont fontStyle = QFont();

		//regular
		//fontStyle.fromString("Arial,30,-1,5,50,0,0,0,0,0");	//using pointSize instead of pixelSize
		fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0");
		fontStyleManager << fontStyle;

		//bold
		fontStyle.fromString("Arial,-1,30,5,75,0,0,0,0,0");
		fontStyleManager << fontStyle;

		//bold + italic
		fontStyle.fromString("Arial,-1,30,5,75,1,0,0,0,0");
		fontStyleManager << fontStyle;

		//italic
		fontStyle.fromString("Arial,-1,30,5,50,1,0,0,0,0");
		fontStyleManager << fontStyle;

		return fontStyleManager;
	}

	QVector<QFont> FontDataGenerator::generateFonts(int fontCount, QStringList fontFamilies, QVector<QFont> fontStyles) {

		if (fontFamilies.isEmpty())
			fontFamilies.append({ "Arial" , "Franklin Gothic Medium" , "Times New Roman" , "Georgia" });

		if (fontStyles.isEmpty())
			fontStyles = generateFontStyles();

		int maxFontNum = (int)fontFamilies.size()*fontStyles.size();

		if (maxFontNum < fontCount) {
			qWarning() << "Font generation process unable to create required number of distinct fonts.";
			qInfo() << "Creating " << maxFontNum << " instead of " << fontCount;
			fontCount = maxFontNum;
		}

		std::vector<int> fontIndices;
		for (int i = 0; i < maxFontNum; ++i)
			fontIndices.push_back(i);

		//shuffle font indices for generation of random fonts
		if (fontCount < maxFontNum)
			std::random_shuffle(fontIndices.begin(), fontIndices.end());

		QVector<QFont>  synthFonts;
		for (int i = 0; i < fontCount; ++i) {
			int idx = fontIndices[i];
			int famInd = (int)floor(idx / fontStyles.size());
			int styleInd = idx - (famInd * fontStyles.size());

			QFont tmp = fontStyles[styleInd];
			tmp.setFamily(fontFamilies[famInd]);
			synthFonts << tmp;
		}

		return synthFonts;
	}

	LabelManager FontDataGenerator::generateFontLabelManager(QVector<QFont> fonts)  {

		LabelManager labelManager = LabelManager();
		QVector<QString> labelNames;
		QVector<LabelInfo> fontLabels;

		if (fonts.isEmpty()) {
			qWarning() << "Missing fonts for creation of label manager";
			return labelManager;
		}

		for (int i = 0; i < fonts.size(); i++) {
			QString labelName = FontStyleClassification::fontToLabelName(fonts[i]);
			LabelInfo label(i + 1, labelName);

			labelNames << labelName;
			labelManager.add(label);
		}

		return labelManager;
	}

	QVector<QSharedPointer<TextPatch>> FontDataGenerator::generateDirTextPatches(QString dirPath, int patchHeight, int textureSize, QSharedPointer<LabelManager> lm) {

		QFileInfoList fileInfoList = Utils::getImageList(dirPath);

		QVector<QSharedPointer<TextPatch>> textPatches = QVector<QSharedPointer<TextPatch>>();

		int i = 0;
		for (auto f : fileInfoList) {

			QString imagePath = f.absoluteFilePath();
			auto imagePatches = generateTextPatches(imagePath, patchHeight, lm, textureSize);

			if (imagePatches.isEmpty()) {
				qWarning() << "No text patches generated! Skipping image.";
				continue;
			}
			i++;
			textPatches << imagePatches;

			//qDebug() << "loaded "<< imagePatches.size() << " text patches from image #" << i;
		}

		qDebug() << "Loaded " << textPatches.size() << " text patches from " << i << " images overall.";

		return textPatches;
	}

	QVector<QSharedPointer<TextPatch>> FontDataGenerator::generateTextPatches(QString imagePath, int patchHeight, QSharedPointer<LabelManager> lm, int textureSize) {

		QVector<QSharedPointer<TextPatch>> textPatches = QVector<QSharedPointer<TextPatch>>();

		QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(imagePath);

		QImage qImg(imagePath);

		if (qImg.isNull() || !QFileInfo(xmlPath).exists()) {
			qWarning() << "Could NOT load image or xml for file: " << imagePath;
			return textPatches;
		}

		cv::Mat imgCv = Image::qImage2Mat(qImg);

		QVector<QSharedPointer<TextLine>> wordRegions = FontDataGenerator::loadRegions<TextLine>(imagePath, Region::type_word);

		if (wordRegions.isEmpty()) {
			qWarning() << "No word regions found, could not generate text patches for image: " << imagePath;
			return textPatches;
		}

		textPatches << FontDataGenerator::generateTextPatches(wordRegions, imgCv, lm, patchHeight, textureSize);	//used for word regions from catalogue images

		return textPatches;
	}

	QVector<QSharedPointer<TextPatch>> FontDataGenerator::generateTextPatches(QStringList textSamples, LabelManager labelManager, int patchHeight, int textureSize) {

		if (textSamples.isEmpty()) {
			qCritical() << "Found no text samples, could not compute text patches.";
			return QVector<QSharedPointer<TextPatch>>();
		}

		if (labelManager.isEmpty()) {
			qCritical() << "Failed to generate test patches.";
			qWarning() << "Label manager is empty.";
			return QVector<QSharedPointer<TextPatch>>();
		}

		//filter font style labels contained in label manager
		auto labels_ = labelManager.labelInfos();
		QVector<LabelInfo> fontStyleLabels;

		for (LabelInfo l : labels_) {
			if (l.name().startsWith("fsl"))
				fontStyleLabels << l;
		}

		QVector<QSharedPointer<TextPatch>> textPatches;
		for (auto l : fontStyleLabels) {

			for (auto s : textSamples) {
				QSharedPointer<TextPatch> tp = QSharedPointer<TextPatch>::create(s, l, patchHeight, textureSize);

				if (!tp->isEmpty()) {
					textPatches << tp;
				}
			}
		}

		qInfo() << "Computed " << textPatches.size() << " text patches.";

		return textPatches;
	}

	QVector<QSharedPointer<TextPatch>> FontDataGenerator::generateTextPatches(QVector<QSharedPointer<TextLine>> wordRegions, cv::Mat img,
		QSharedPointer<LabelManager> lm, int patchHeight, int textureSize) {

		QVector<QSharedPointer<TextPatch>> textPatches = QVector<QSharedPointer<TextPatch>>();

		//TODO enable generation of patches without labels
		//TODO merge this function with adaptPatchHeight() in FSC class
		//TODO improve reading in of labels (allow labels containing spaces)

		if (wordRegions.isEmpty()) {
			qWarning() << "No regions to convert here.";
			return textPatches;
		}

		Rect imgRec = Rect(img);
		for (QSharedPointer<TextLine> wr : wordRegions) {

			//TODO add function for finding label in string (other class)
			//TODO adapt to new font style label
			//check if word region has a font style label
			QString cs = wr->custom();
			QStringList list = cs.split("\\b");
			//qDebug() << list;

			QString trLabelName;
			for (auto s : list) {
				if (s.contains("fsl")) {
					trLabelName = s;
					break;
				}
			}

			//TODO consider processing of patch without valid font style label
			if (trLabelName.isNull()) {
				continue;
			}

			LabelInfo trLabel = lm->find(trLabelName);

			if (trLabel.isNull()) {
				trLabel = LabelInfo(lm->size(), trLabelName);
				lm->add(trLabel);
			}

			Rect tpRect = Rect::fromPoints(wr->polygon().toPoints());

			//generate text patch
			QSharedPointer<TextPatch> tp = QSharedPointer<TextPatch>::create(img, tpRect, patchHeight, textureSize);
			if (tp->isEmpty()) {
				qWarning() << "Failed to create valid text patch! Skipping region.";
				continue;
			}

			tp->label()->setTrueLabel(trLabel);
			tp->setPolygon(wr->polygon());
			textPatches << tp;
		}

		return textPatches;
	}

	FeatureCollectionManager FontDataGenerator::computePatchFeatures(QVector<QSharedPointer<TextPatch>> textPatches, 
		GaborFilterBank gfb, bool addLabels) {

		qInfo() << "Computing features for text patches. This might take a while...";
		cv::Mat tpFeatures = FontStyleClassification::computeGaborFeatures(textPatches, gfb);

		//save feature collection manager
		FeatureCollectionManager fcm;

		if (addLabels)	//one collection per label -> order of feature vectors is changed
			fcm = FontStyleClassification::generateFCM(textPatches, tpFeatures);
		else			//single collection with unknown label -> order of feature vectors unchanged
			fcm = FontStyleClassification::generateFCM(tpFeatures);

		return fcm;
	}

	bool FontDataGenerator::generateDataSet(QStringList samples, QVector<QFont> fonts, GaborFilterBank gfb, 
		QString outputFilePath, int patchHeight, int textureSize, bool addLabels) {

		//TODO write gfb parameters to output file

		Timer dt;

		if (QFileInfo(outputFilePath).exists()) {
			qWarning() << "Data set already exists. Delete existing files to generate new one:" << outputFilePath;
			return false;
		}

		LabelManager labelManager = FontDataGenerator::generateFontLabelManager(fonts);

		if (samples.isEmpty() || labelManager.isEmpty()) {
			qCritical() << "Could not generate data set, missing input data.";
			return false;
		}

		QVector<QSharedPointer<TextPatch>> textPatches = FontDataGenerator::generateTextPatches(samples, labelManager, patchHeight, textureSize);
		FeatureCollectionManager fcm = FontDataGenerator::computePatchFeatures(textPatches, gfb, addLabels);

		//write data set to file
		QJsonObject jo = fcm.toJson(outputFilePath);

		QJsonArray ja = QJsonArray::fromStringList(samples);
		jo.insert("wordSamples", ja);

		Utils::writeJson(outputFilePath, jo);

		qDebug() << "Generated data set file in " << dt;

		return true;
	}

	//generates data set from image/xml files in specified folder
	bool FontDataGenerator::generateDataSet(QString dataSetPath, GaborFilterBank gfb, int patchHeight, int textureSize, QString outputFilePath, bool addLabels) {

		Timer dt;

		//generate train data set from image files
		QVector<QSharedPointer<TextPatch>> textPatches = FontDataGenerator::generateDirTextPatches(dataSetPath, patchHeight, textureSize);

		//debug images
		//cv::Mat debugPatches = FontDataGenerator::drawTextPatches(textPatches);
		//cv::Mat debugPatchTextures = FontDataGenerator::drawTextPatches(textPatches, true);

		if (textPatches.isEmpty()) {
			qCritical() << "Failed to load text patches from directory: " << dataSetPath;
			return false;
		}

		auto fcm = FontDataGenerator::computePatchFeatures(textPatches, gfb, addLabels);

		//write data set to file
		if (outputFilePath.isEmpty())
			outputFilePath = QFileInfo(dataSetPath, "FontStyleDataSet.txt").absoluteFilePath();

		FontStyleDataSet fsd = FontStyleDataSet(fcm, patchHeight, gfb);
		
		if(!fsd.write(outputFilePath)){
			qCritical() << "Failed to write data set to path: " << outputFilePath;
			return false;
		}

		qDebug() << "Generated data set file in " << dt;

		return true;
	}

	bool FontDataGenerator::readDataSet(QString inputFilePath, FeatureCollectionManager& fcm, QStringList& samples) {

		//read fcm
		fcm = FeatureCollectionManager::read(inputFilePath);

		//read samples
		QJsonArray sampleJA = Utils::readJson(inputFilePath).value("wordSamples").toArray();

		if (sampleJA.isEmpty() || fcm.isEmpty()) {
			qCritical() << "Failed to load data set from: " << inputFilePath;
			return false;
		}

		for (auto s : sampleJA)
			samples << s.toString();

		qInfo() << "Successfully loaded data set from: " << inputFilePath;
		return true;
	}

	cv::Mat FontDataGenerator::drawTextPatches(QVector<QSharedPointer<TextPatch>> patches, bool drawPatchTexture){
		
		int maxWidth = 1500;
		int maxHeight = 10000;
		cv::Mat output = cv::Mat();

		if(patches.isEmpty())
			return output;

		cv::Mat outputLine = cv::Mat();
		for (auto p : patches) {
			if (p->isEmpty())
				continue;

			cv::Mat patchImg;
			if (drawPatchTexture)
				patchImg = p->patchTexture();
			else
				patchImg = p->textPatchImg();
			

			if (outputLine.empty()) {
				outputLine = patchImg.clone();
				if (outputLine.cols > maxWidth) {
					qWarning() << "Failed to create output image. Patch width bigger than 1500 pixels.";
					return cv::Mat();
				}
			}
			else {
				
				if (patchImg.rows != outputLine.rows) {
					qWarning() << "Failed to create output image. Patches have varying heights.";
					return cv::Mat();
				}
			
				if (outputLine.cols + patchImg.cols > 1500) {

					cv::copyMakeBorder(outputLine, outputLine, 0, 0, 0, maxWidth - outputLine.cols, cv::BORDER_CONSTANT, cv::Scalar(255));

					if (output.empty())
						output = outputLine.clone();
					else {
						cv::vconcat(output, outputLine, output);

						if (outputLine.cols > maxWidth) {
							qWarning() << "Failed to create output image. Patch width bigger than 1500 pixels.";
							return cv::Mat();
						}
					}	

					outputLine = patchImg.clone();
				}
				else {
					cv::hconcat(outputLine, patchImg, outputLine);
				}
			}

			if (output.rows > maxHeight) {
				qWarning() << "Aborted generation of text patch images. Output image exceeds height of " << maxHeight << " pixels.";
				return output;
			}
		}

		//add last line
		if (!outputLine.empty()) {
			cv::copyMakeBorder(outputLine, outputLine, 0, 0, 0, maxWidth - outputLine.cols, cv::BORDER_CONSTANT, cv::Scalar(255));
			cv::vconcat(output, outputLine, output);
		}

		return output;
	}

	// FontStyleDataSet -------------------------------------------------------------------------------------

	FontStyleDataSet::FontStyleDataSet() {
		setGFB(GaborFilterBank());
		setFCM(FeatureCollectionManager());
	}

	FontStyleDataSet::FontStyleDataSet(FeatureCollectionManager fcm, int patchHeight, GaborFilterBank gfb) {
		setGFB(gfb);
		setFCM(fcm);
		setPatchHeight(patchHeight);
	}

	bool FontStyleDataSet::isEmpty() const {
		if (mGFB.isEmpty() || mFCM.isEmpty())
			return true;

		return false;
	}

	int FontStyleDataSet::patchHeight() const{
		return mPatchHeight;
	}

	GaborFilterBank FontStyleDataSet::gaborFilterBank() const {
		return mGFB;
	}

	FeatureCollectionManager FontStyleDataSet::featureCollectionManager() const {
		return mFCM;
	}

	LabelManager FontStyleDataSet::labelManager() const {
		return mLM;
	}

	void FontStyleDataSet::setPatchHeight(int patchHeight){
		mPatchHeight = patchHeight;
	}

	void FontStyleDataSet::setGFB(GaborFilterBank gfb) {
		mGFB = gfb;
	}

	void FontStyleDataSet::setFCM(FeatureCollectionManager & fcm) {
		mFCM = fcm;
		mLM = fcm.toLabelManager();
	}

	//TODO test read/write/json functions for font style data set
	void FontStyleDataSet::toJson(QJsonObject & jo, const QString & filePath) const {
		
		QJsonObject jod;

		QJsonArray ja = mFCM.toJson(filePath).value(FeatureCollection::jsonKey()).toArray();
		jod.insert(FeatureCollection::jsonKey(), ja);

		mGFB.toJson(jod);
		jod.insert("patchHeight", mPatchHeight);

		jo.insert(jsonKey(), jod);
	}

	bool FontStyleDataSet::write(const QString & filePath) const{

		QJsonObject jo;
		toJson(jo, filePath);

		int64 bw = Utils::writeJson(filePath, jo);

		return bw > 0;
	}

	FontStyleDataSet FontStyleDataSet::fromJson(const QJsonObject & jo, const QString & filePath) {
		
		jo.value(jsonKey()).toObject();
		FeatureCollectionManager fcm = FeatureCollectionManager::fromJson(jo, filePath);
		GaborFilterBank gfb = GaborFilterBank::fromJson(jo.value(GaborFilterBank::jsonKey()).toObject());
		int patchHeight = jo.value("patchHeight").toInt();
		FontStyleDataSet fsd = FontStyleDataSet(fcm, patchHeight, gfb);

		if (fsd.isEmpty())
			qCritical() << "Failed to load font style data set from Json.";

		return fsd;
	}

	FontStyleDataSet FontStyleDataSet::read(const QString & filePath) {

		FontStyleDataSet fsd;

		QJsonObject jo = Utils::readJson(filePath).value(FontStyleDataSet::jsonKey()).toObject();
		
		if (jo.empty())
			qCritical() << "Failed to load "<< jsonKey() << " JsonObject from file: " << filePath;
		else
			fsd = fromJson(jo, filePath);

		return fsd;
	}

	QString FontStyleDataSet::jsonKey() {
		return "FontStyleDataSet";
	}

	// FontStyleClassifier --------------------------------------------------------------------
	FontStyleClassifier::FontStyleClassifier(FontStyleDataSet dataSet, const cv::Ptr<cv::ml::StatModel> model, int classifierMode) {
		mModel = model;
		mFCM = dataSet.featureCollectionManager();
		mGFB = dataSet.gaborFilterBank();
		mClassifierMode = (ClassifierMode)classifierMode;
	}

	bool FontStyleClassifier::isEmpty() const {
		return mModel->empty() || mFCM.isEmpty();
	}

	bool FontStyleClassifier::isTrained() const {
		return mModel->isTrained();
	}

	cv::Ptr<cv::ml::StatModel> FontStyleClassifier::model() const {
		return mModel;
	}

	LabelManager FontStyleClassifier::manager() const {
		return mFCM.toLabelManager();
	}

	GaborFilterBank FontStyleClassifier::gaborFilterBank() const{
		return mGFB;
	}

	QVector<LabelInfo> FontStyleClassifier::classify(cv::Mat testFeat) {

		//TODO compute output probability results for each class
		//TODO consider using additional weights

		if (!checkInput())
			return QVector<LabelInfo>();

		cv::Mat cFeatures = testFeat;
		cFeatures.convertTo(cFeatures, CV_32FC1);

		float rawLabel = 0;
		QVector<LabelInfo> labelInfos;
		LabelManager labelManager = mFCM.toLabelManager();

		cv::Mat featStdDev;
		QVector<cv::Mat> cCentroids;
		if (mClassifierMode == ClassifierMode::classify_nn || mClassifierMode == ClassifierMode::classify_nn_wed) {
			cCentroids = mFCM.collectionCentroids();
			featStdDev = mFCM.featureSTD();
		}

		for (int rIdx = 0; rIdx < cFeatures.rows; rIdx++) {
			cv::Mat cr = cFeatures.row(rIdx);

			if (svm()) {
				rawLabel = svm()->predict(cr);
			}
			else if (bayes()) {
				rawLabel = bayes()->predict(cr);
				//bayes()->predictProb(InputArray inputs, OutputArray outputs, OutputArray outputProbs, int flags = 0);
			}
			else {
				if (mClassifierMode == ClassifierMode::classify_nn || mClassifierMode == ClassifierMode::classify_knn) {
					rawLabel = kNearest()->predict(cr);
				}
				else if (mClassifierMode == ClassifierMode::classify_nn_wed) {

					//compute weighted euclidean distance by normalizing features by their standard deviation
					cv::divide(cr, featStdDev, cr);
					rawLabel = kNearest()->predict(cr);

					//TODO compute and save detailed responses
					//cv::Mat results, responses, dists;
					//rawLabel = kNearest()->findNearest(cr, kNearest()->getDefaultK(), results, responses, dists);
				}
				else {
					qCritical() << "Unable to perform font style classification. Classifier mode is unknown.";
					return QVector<LabelInfo>();
				}
			}

			// get label
			int labelId = qRound(rawLabel);
			LabelInfo li = labelManager.find(labelId);
			labelInfos << li;
		}

		return labelInfos;
	}

	cv::Ptr<cv::ml::SVM> FontStyleClassifier::svm() const {
		return mModel.dynamicCast<cv::ml::SVM>();
	}

	cv::Ptr<cv::ml::NormalBayesClassifier> FontStyleClassifier::bayes() const {
		return mModel.dynamicCast<cv::ml::NormalBayesClassifier>();
	}

	cv::Ptr<cv::ml::KNearest> FontStyleClassifier::kNearest() const {
		return mModel.dynamicCast<cv::ml::KNearest>();
	}

	cv::Mat FontStyleClassifier::draw(const cv::Mat& img) const {

		if (!checkInput())
			return cv::Mat();

		QImage qImg = Image::mat2QImage(img, true);
		QPainter p(&qImg);

		// draw legend
		mFCM.toLabelManager().draw(p);

		return Image::qImage2Mat(qImg);
	}

	bool FontStyleClassifier::checkInput() const {

		if (!isEmpty() && !isTrained())
			qCritical() << "I cannot classify, since the model is not trained";

		return !isEmpty() && isTrained();
	}

	bool FontStyleClassifier::write(const QString & filePath) const {

		if (mModel && !mModel->isTrained())
			qWarning() << "Writing classifier that is NOT trained!";

		// write classifier model
		QJsonObject jo;
		toJson(jo, filePath);
		int64 bw = Utils::writeJson(filePath, jo);

		return bw > 0;	// if we wrote more than 0 bytes, it's ok
	}

	void FontStyleClassifier::toJson(QJsonObject& jo, QString filePath) const {

		if (!mModel) {
			qWarning() << "cannot save FontStyleClassifier because statModel is NULL.";
			return;
		}

		// write features
		QJsonObject joc = mFCM.toJson(filePath);
		jo.insert(FeatureCollection::jsonKey(), joc.value(FeatureCollection::jsonKey()));

		cv::FileStorage fs(".xml", cv::FileStorage::WRITE | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_XML);
		mModel->write(fs);
#if CV_MAJOR_VERSION == 3 && CV_MINOR_VERSION == 1
		fs << "format" << 3;	// fixes bug #4402
#endif
		std::string data = fs.releaseAndGetString();

		QByteArray ba(data.c_str(), (int)data.length());
		QString ba64Str = ba.toBase64();

		jo.insert("ClassifierModel", ba64Str);
		jo.insert("ClassifierMode", QJsonValue(mClassifierMode));
		mGFB.toJson(jo);
	}

	QString FontStyleClassifier::jsonKey() const {
		return "FontStyleClassifier";
	}

	QSharedPointer<FontStyleClassifier> FontStyleClassifier::read(const QString & filePath) {

		Timer dt;

		QJsonObject jo = Utils::readJson(filePath);

		if (jo.isEmpty()) {
			qCritical() << "Failed to load font style classifier from" << filePath;
			return QSharedPointer<FontStyleClassifier>::create();
		}

		QSharedPointer<FontStyleClassifier> fsc = QSharedPointer<FontStyleClassifier>::create();
		auto fcm = FeatureCollectionManager::read(filePath);
		fsc->mFCM = fcm;

		if (jo.contains("ClassifierMode"))
			fsc->mClassifierMode = (ClassifierMode)jo.value("ClassifierMode").toInt();
		else
			fsc->mClassifierMode = (ClassifierMode)-1;

		fsc->mModel = FontStyleClassifier::readStatModel(jo, fsc->mClassifierMode);

		if (!fsc->mFCM.isEmpty() && !fsc->mModel->empty()) {
			qInfo() << "Font style classifier loaded from" << filePath << "in" << dt;
		}
		else {
			qCritical() << "Failed to load font style classifier from" << filePath;
			return QSharedPointer<FontStyleClassifier>::create();
		}

		fsc->mGFB = GaborFilterBank::fromJson(jo.value(GaborFilterBank::jsonKey()).toObject());

		return fsc;
	}

	cv::Ptr<cv::ml::StatModel> FontStyleClassifier::readStatModel(QJsonObject & jo, ClassifierMode mode) {

		// decode data
		QByteArray ba = jo.value("ClassifierModel").toVariant().toByteArray();
		ba = QByteArray::fromBase64(ba);

		if (!ba.length()) {
			qCritical() << "Can not read font style classifier from file.";
			return cv::Ptr<cv::ml::StatModel>();
		}

		// read model from memory
		cv::String dataStr(ba.data(), ba.length());
		cv::FileStorage fs(dataStr, cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_XML);
		cv::FileNode root = fs.root();

		if (root.empty()) {
			qCritical() << "Can not read font style classifier model from file";
			return cv::Ptr<cv::ml::StatModel>();
		}

		cv::Ptr<cv::ml::StatModel> model;
		if (mode == FontStyleClassifier::classify_bayes)
			model = cv::Algorithm::read<cv::ml::NormalBayesClassifier>(root);
		else if (mode == FontStyleClassifier::classify_svm)
			model = cv::Algorithm::read<cv::ml::SVM>(root);
		else if (mode == classify_knn || mode == classify_nn || mode == classify_nn_wed)
			model = cv::Algorithm::read<cv::ml::KNearest>(root);
		else {
			qCritical() << "Can not read font style classifier from file. Classifier mode unknown.";
			return cv::Ptr<cv::ml::StatModel>();
		}

		return model;
	}

	// FontStyleClassificationConfig --------------------------------------------------------------------
	FontStyleClassificationConfig::FontStyleClassificationConfig() : ModuleConfig("Font Style Classification Module") {
	}

	QString FontStyleClassificationConfig::toString() const {
		return ModuleConfig::toString();
	}

	void FontStyleClassificationConfig::setTestBool(bool testBool) {
		mTestBool = testBool;
	}

	bool FontStyleClassificationConfig::testBool() const {
		return mTestBool;
	}

	void FontStyleClassificationConfig::setTestInt(int testInt) {
		mTestInt = testInt;
	}

	int FontStyleClassificationConfig::testInt() const {
		return ModuleConfig::checkParam(mTestInt, 0, INT_MAX, "testInt");
	}

	void FontStyleClassificationConfig::setTestPath(const QString & tp) {
		mTestPath = tp;
	}

	QString FontStyleClassificationConfig::testPath() const {
		return mTestPath;
	}

	void FontStyleClassificationConfig::load(const QSettings & settings) {

		mTestBool = settings.value("testBool", testBool()).toBool();
		mTestInt = settings.value("testInt", testInt()).toInt();		
		mTestPath = settings.value("classifierPath", testPath()).toString();
	}

	void FontStyleClassificationConfig::save(QSettings & settings) const {

		settings.setValue("testBool", testBool());
		settings.setValue("testInt", testInt());
		settings.setValue("testPath", testPath());
	}

	// FontStyleClassification --------------------------------------------------------------------
	FontStyleClassification::FontStyleClassification() {
		mClassifier = QSharedPointer<FontStyleClassifier>::create();
		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();
	}

	FontStyleClassification::FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines) {
		mImg = img;
		mTextLines = textLines;
		mProcessLines = true;
		mClassifier = QSharedPointer<FontStyleClassifier>::create();

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();

		mScaleFactory = QSharedPointer<ScaleFactory>(new ScaleFactory(img.size()));
	}

	FontStyleClassification::FontStyleClassification(const QVector<QSharedPointer<TextPatch>>& textPatches, QString featureFilePath) {
		mProcessLines = false;
		mTextPatches = textPatches;
		mFeatureFilePath = featureFilePath;
		mClassifier = QSharedPointer<FontStyleClassifier>::create();

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();
	}

	bool FontStyleClassification::isEmpty() const {
		return (mProcessLines && (mImg.empty() || mTextLines.isEmpty())) || (!mProcessLines && mTextPatches.isEmpty());
	}

	bool FontStyleClassification::compute() {

		//TODO text line processing: convert patches to text (line) regions for xml output

		if (!checkInput())
			return false;

		mGfb = mClassifier->gaborFilterBank();
		
		if (mGfb.isEmpty()) {
			qCritical() << "Gabor filter bank undefined, make sure classifier file contains parameter information!";
			return false;
		}

		if (mProcessLines) {
			cv::Mat img = mImg.clone();

			//debug
			//QImage patchResults = Image::mat2QImage(img, true);
			//QPainter painter(&patchResults);

			//rotate text lines according to baseline orientation and crop its image
			for (auto tl : mTextLines) {

				//rotate text line patch according to baseline angle
				Line baseline(Polygon(tl->baseLine().toPolygon()));

				if (baseline.isEmpty()) {
					qWarning() << "Failed to process text line. Missing base line information.";
					continue;
				}

				double angleDeg = baseline.angle() * (180.0 / CV_PI);
				Vector2D center = baseline.center();

				cv::Mat imgRot = cv::Mat(img.size(), CV_8UC4, cv::Scalar(255, 255, 255, 255));
				if (angleDeg != 0) {
					cv::Mat rot_mat = cv::getRotationMatrix2D(center.toCvPoint(), angleDeg, 1);
					cv::warpAffine(img, imgRot, rot_mat, img.size());
				}
				else
					imgRot = img;
				
				//rotate text region polygon
				Polygon poly = tl->polygon();
				poly.rotate(baseline.angle(), center);

				//find bounding box of rotated text region polygon
				cv::Rect bb = Rect::fromPoints(poly.toPoints()).toCvRect();
				//cv::cvtColor(imgRot, imgRot, cv::COLOR_BGRA2GRAY);
				cv::Mat croppedImage = imgRot(bb);

				//split text line into text patches
				auto textPatches = splitTextLine(croppedImage, bb);

				if (textPatches.isEmpty())
					continue;

				//rotate patch polygons back to original image coordinates
				for (auto tp : textPatches) {
					Polygon tpPoly = tp->polygon();
					tpPoly.rotate(-baseline.angle(), center);
					tp->setPolygon(tpPoly);
				}

				//gather extracted patches for classification
				mTextPatches.append(textPatches);
			}

			//compute classification results 
			if (!processPatches())
				qCritical() << "Failed to classify style of text lines.";
		}
		else {
			if (!processPatches())
				return false;
		}

		return true;
	}

	void FontStyleClassification::setClassifier(const QSharedPointer<FontStyleClassifier>& classifier){
		mClassifier = classifier;
	}

	QVector<QSharedPointer<TextPatch>> FontStyleClassification::splitTextLine(cv::Mat lineImg, Rect bbox) {
		
		//TODO add additional checks for more robustness 
		//TODO check against:	single word lines
		//						very short words,
		//						little difference between gap clusters
		//						very large gaps that need to be removed
		//						difference in line height (ascenders, descender, font size change)
		// use connected component information to improve gap detection
		// strengthen regions representing one connected component

		//convert input image to gray scale
		cv::Mat lineImg_ = IP::grayscale(lineImg);

		cv::Mat vPP;
		bitwise_not(lineImg_, lineImg_);
		reduce(lineImg_, vPP, 0, cv::REDUCE_SUM, CV_32F);
		vPP = vPP / 255; //normalize
		cv::Mat vPPRawImg = Utils::drawBarChart(vPP);

		GaussianBlur(vPP, vPP, cv::Size(5, 1), 0, 0, cv::BORDER_DEFAULT);
		cv::Mat vPPImg = Utils::drawBarChart(vPP);

		//prune vertical projection profile
		QList<double> values;
		for (int i = 0; i < vPP.cols; ++i)
			values << (double)vPP.at<float>(i);

		double q = 0.10;
		double qValue = Algorithms::statMoment(values, q);
		cv::Mat prunedVPP = cv::Mat::zeros(vPP.size(), vPP.type());
		vPP.copyTo(prunedVPP, (vPP > (float)qValue));
		cv::Mat gaps = prunedVPP == 0;

		//debugging: visualize line, vpp and gaps
		//cv::Mat prunedVPPImg = Utils::drawBarChart(prunedVPP);
		//cv::Mat gapsImg = Utils::drawBarChart((gaps / 255) * 20);
		//cv::Mat results;
		//cv::vconcat(lineImg, vPPImg, results);
		//cv::vconcat(results, gapsImg, results);

		//compute white spaces
		QVector<cv::Range> whiteSpaces;
		int start = 0;
		bool activeRun = false;

		for (int i = 0; i < gaps.cols; ++i) {

			if (!activeRun && gaps.at<uchar>(i) == 255) {
				start = i;
				activeRun = true;
			}

			if (activeRun && gaps.at<uchar>(i) == 0) {
				whiteSpaces << cv::Range(start, i);
				activeRun = false;
			}
		}

		//add trailing ws
		if (activeRun) {
			whiteSpaces << cv::Range(start, gaps.cols);
		}

		//cluster white spaces in two groups
		cv::Mat labels;
		std::vector<cv::Point2f> centers, data;

		for (auto ws : whiteSpaces)
			data.push_back(cv::Point2f((float)ws.size(), 0.0f));

		cv::kmeans(data, 2, labels, cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0), 3, cv::KMEANS_RANDOM_CENTERS, centers);

		//debug gap cluster centers
		//for (auto c : centers)
		//	qDebug() << "" << Vector2D(c).toString();

		QVector<cv::Range> final_whiteSpaces;
		int bigGapIdx = (centers[0].x > centers[1].x) ? 0 : 1;
		cv::Mat textPatchImg = cv::Mat(vPP.size(), CV_8UC1, cv::Scalar(255));
		
		for (int i = 0; i < data.size(); ++i) {
			if (labels.at<int>(i) == bigGapIdx) {
				final_whiteSpaces << whiteSpaces[i];
				textPatchImg(cv::Range(0, 1), whiteSpaces.at(i)) = 0;
			}
		}
				
		//compute text patch regions
		activeRun = false;
		QVector<Rect> patchRects;
		int height = lineImg.size().height - 1;

		for (int i = 0; i < textPatchImg.cols; ++i) {
			if (!activeRun && textPatchImg.at<uchar>(i) == 255) {
				start = i;
				activeRun = true;
			}

			if (activeRun && textPatchImg.at<uchar>(i) == 0) {		
				int width = (i - start);
				activeRun = false;

				if (width > 0) {
					Rect patchRect = Rect(start, 0, width, height);
					patchRects << patchRect;
				}
			}
		}
		
		if (activeRun) { //add trailing patch
			int width = (textPatchImg.cols - start);
			patchRects << Rect(start, 0, width, height);
		}

		//generate text patches
		QVector<QSharedPointer<TextPatch>> textPatches;
		for (auto pr : patchRects) {
			auto tp = QSharedPointer<TextPatch>::create(lineImg(pr.toCvRect()));
			pr.move(bbox.topLeft());
			tp->setPolygon(Polygon::fromRect(pr));
			textPatches << tp;
		}
		
		if (textPatches.isEmpty()) {
			qCritical() << "Failed to split text line into text patches.";
			return textPatches;
		}

		////visualize final text patches extracted from text line
		//QImage patchResults = Image::mat2QImage(lineImg, true);
		//QPainter painter(&patchResults);

		//for (int i = 0; i < patchRects.size(); ++i) {
		//	painter.setPen(ColorManager::blue());
		//	painter.drawRect(patchRects[i].toQRect());
		//}

		//cv::Mat patchResultsCV = Image::qImage2Mat(patchResults);

		return textPatches;
	}

	bool FontStyleClassification::processPatches(){

		//get test features
		cv::Mat features;
		if (!loadFeatures()) {
			features = computeGaborFeatures(mTextPatches, mGfb);
			mFCM_test = generateFCM(features);	//do not pass additional patches with GT labels (if available)
		}

		if (mFCM_test.isEmpty())
			return false;

		//convert features to CvTrainData format
		cv::Mat testFeatures;
		testFeatures = mFCM_test.toCvTrainData(-1, false)->getSamples(); //do not use additional normalization
		testFeatures.convertTo(testFeatures, CV_64F);

		//compute classification results for test features
		QVector<LabelInfo> cLabels = mClassifier->classify(testFeatures);

		if (mTextPatches.size() != cLabels.size()) {
			qCritical() << "Failed to classify text patches.";
			qInfo() << "Number of test samples is out of sync with number of result labels.";
			return false;
		}

		for (int idx = 0; idx < mTextPatches.size(); idx++) {
			auto label = mTextPatches[idx]->label();
			label->setLabel(cLabels[idx]);
		}

		return true;
	}

	bool FontStyleClassification::mapStyleToPatches(QVector<QSharedPointer<TextPatch>>& regionPatches) const {

		if (!checkInput()) {
			qWarning() << "Failed to map styles to patches.";
			qInfo() << "Make sure font style classification module is set up correctly.";
			return false;
		}

		cv::Mat styleMap = labelMap();
		if (styleMap.empty()) {
			qWarning() << "No font style classification results found!";
			return false;
		}

		LabelManager lm = mClassifier->manager();

		double maxVal;
		cv::minMaxLoc(styleMap, NULL, &maxVal, NULL, NULL);

		for (auto rp : regionPatches) {

			Polygon poly = rp->polygon();
			std::vector<cv::Point> points = poly.toCvPoints();

			cv::Mat mask = cv::Mat::zeros(styleMap.size(), CV_8UC1);
			std::vector<cv::Point> polyPoints = poly.toCvPoints();
			cv::fillConvexPoly(mask, polyPoints, cv::Scalar(1), cv::LINE_8, 0);
			cv::Mat rpLabels = mask.mul(styleMap);

			std::vector<int> labelCounter;
			labelCounter.push_back(1);
			for (int li = 1; li <= maxVal; ++li) {
				cv::Mat rplm = rpLabels == li;
				int lCount = cv::countNonZero(rplm);
				labelCounter.push_back(lCount);
				//qDebug() << "count for label index: " << li << " = " << lCount;
			}

			cv::Point maxPos;
			cv::minMaxLoc(labelCounter, NULL, NULL, NULL, &maxPos);
			int resultLabelID = maxPos.x;
			//qDebug() << "final label has id: " << maxPos.x << ", " << maxPos.y;

			//set result label to region patch label
			auto label = rp->label();
			label->setLabel(lm.find(resultLabelID));
		}

		return true;
	}

	bool FontStyleClassification::loadFeatures(){
		
		if (!mFeatureFilePath.isEmpty()) {
			mFCM_test = FeatureCollectionManager::read(mFeatureFilePath);

			if (mFCM_test.numFeatures() != mTextPatches.size()) {
				qWarning() << "Number of loaded feature vectors does not match number of input text patches.";
				qInfo() << "Feature vectors need to be recomputed.";
				return false;
			}
		}
		else {
			return false;
		}

		return true;
	}

	QString FontStyleClassification::fontToLabelName(QFont font){

		//TODO include font size property in labelName
		//TODO add additional property for flagging label as GT

		QString labelName = "fsl[";
		labelName += font.family() + ";";

		if (font.bold())
			labelName += "b;";
		else
			labelName += "!b;";

		if (font.italic())
			labelName += "i;";
		else
			labelName += "!i;";

		labelName += "s" + QString::number(font.pixelSize());

		labelName += "]";

		return labelName;
	}

	QFont FontStyleClassification::labelNameToFont(QString labelName){

		int sIdx = labelName.indexOf("fsl[");

		if (sIdx == -1) {
			qWarning() << "Failed to create font from label name: " << labelName;
			return QFont();
		}

		sIdx += 4;
		int eIdx = labelName.indexOf("]", sIdx);
		QString labelAtt = labelName.mid(sIdx, (eIdx - sIdx));

		QStringList lp = labelAtt.split(";");
		if (lp.size()!= 4) {
			qWarning() << "Failed to create font from label name: " << labelName;
			return QFont();
		}

		QFont font;

		if(lp[1] == "b")
			font.setBold(true);

		if (lp[1] == "!b")
			font.setBold(false);

		if (lp[2] == "i")
			font.setItalic(true);

		if (lp[2] == "!i")
			font.setItalic(false);

		bool ok=false;
		int fSize = 30;

		if (lp[3].at(0)=="s") {
			QString fontSizeStr = lp[3].mid(1, -1);
			fSize = fontSizeStr.toInt(&ok);
		}

		if (!ok) {
			qWarning() << "Failed to create font from label name: " << labelName;
			return QFont();
		} 
		
		font.setPixelSize(fSize);
		font.setFamily(lp[0]);

		return font;
	}

	cv::Mat FontStyleClassification::computeGaborFeatures(QVector<QSharedPointer<TextPatch>> patches, GaborFilterBank gfb, cv::ml::SampleTypes featureType){
		cv::Mat featM;
		for (auto p : patches) {
			cv::Mat features = GaborFiltering::extractGaborFeatures(p->patchTexture(), gfb);
			if (!featM.empty())
				cv::hconcat(featM, features, featM);
			else
				featM = features.clone();
		}

		if (featureType == cv::ml::ROW_SAMPLE)
			cv::transpose(featM, featM);

		return featM;
	}

	FeatureCollectionManager FontStyleClassification::generateFCM(cv::Mat features) {
		
		FeatureCollectionManager fcm = FeatureCollectionManager();
		
		if (!features.empty()) {
			FeatureCollection testDataCollection = FeatureCollection(features, TextPatch().label()->trueLabel());
			fcm.add(testDataCollection);
		}

		return fcm;
	}

	FeatureCollectionManager FontStyleClassification::generateFCM(QVector<QSharedPointer<TextPatch>> patches, cv::Mat features){

		if (patches.size() != features.rows) {
			qCritical() << "Failed to create feature collection manager.";
			qInfo() << "The number of samples does not match the number of features.";
			return FeatureCollectionManager();
		}

		FeatureCollectionManager fcm = FeatureCollectionManager();

		//split text patches according to their labels
		QVector<FeatureCollection> collections;
		for (int idx = 0; idx < patches.size(); idx++) {
			LabelInfo patchLabel = patches[idx]->label()->trueLabel();

			bool isNew = true;
			for (FeatureCollection& fc : collections) {
				if (fc.label() == patchLabel) {
					fc.append(features.row(idx));
					isNew = false;
				}
			}

			if (isNew)
				collections.append(FeatureCollection(features.row(idx), patchLabel));
		}

		for (auto c : collections)
			fcm.add(c);

		return fcm;
	}

	QVector<QSharedPointer<TextPatch>> FontStyleClassification::textPatches() const{
		return mTextPatches;
	}

	cv::Mat FontStyleClassification::labelMap() const{
	
		if (mTextPatches.isEmpty()) {
			qWarning() << "Failed to created label map! No result patches found.";
			return cv::Mat();
		}
		
		Rect bbox;
		for (auto tp : mTextPatches) {
			Polygon poly = tp->polygon();
			if (poly.isEmpty())
				continue;
			bbox = bbox.joined(Rect::fromPoints(poly.toPoints()));
		}

		if (bbox.isNull())
			return cv::Mat();

		cv::Mat predlabelMap((int)bbox.height(), (int)bbox.width(), CV_8UC1, cv::Scalar(0));
		for (auto tp : mTextPatches) {
			auto poly = tp->polygon();

			if (poly.isEmpty())
				continue;
			
			cv::Mat polyImg((int) bbox.height(), (int) bbox.width(), CV_8UC1, cv::Scalar(0));
			cv::fillConvexPoly(polyImg, poly.toCvPoints(), cv::Scalar(tp->label()->predicted().id()), cv::LINE_8, 0);
			cv::Mat mask = polyImg != 0;
			polyImg.copyTo(predlabelMap, mask);
		}

		return predlabelMap;
	}

	QSharedPointer<FontStyleClassificationConfig> FontStyleClassification::config() const {
		return qSharedPointerDynamicCast<FontStyleClassificationConfig>(mConfig);
	}

	cv::Mat FontStyleClassification::draw(const cv::Mat & img) const {

		QImage outputImg = Image::mat2QImage(img, true);
		QPainter painter(&outputImg);

		for (int i = 0; i < mTextPatches.size(); ++i) {
			int predLabelID = mTextPatches[i]->label()->predicted().id();
			QColor predLabelColor = ColorManager::getColor(predLabelID, 0.5);

			painter.setBrush(predLabelColor);
			painter.setPen(predLabelColor);

			painter.drawPolygon(mTextPatches[i]->polygon().polygon());
		}

		cv::Mat outputImgCV = Image::qImage2Mat(outputImg);

		return outputImgCV;
	}

	cv::Mat FontStyleClassification::draw(const cv::Mat & img, QVector<QSharedPointer<TextPatch>> patches, const DrawFlags & options) const {

		QImage outputImg = Image::mat2QImage(img, true);
		QPainter painter(&outputImg);

		if (options & draw_patch_results) {

			if (!mapStyleToPatches(patches)) {
				return img;
			}

			for (int i = 0; i < patches.size(); ++i) {
				int predLabelID = patches[i]->label()->predicted().id();
				QColor predLabelColor = ColorManager::getColor(predLabelID, 0.5);

				painter.setBrush(predLabelColor);
				painter.setPen(predLabelColor);

				painter.drawPolygon(patches[i]->polygon().polygon());
			}
			return Image::qImage2Mat(outputImg);
		}

		if (options & draw_comparison) {

			if (!mapStyleToPatches(patches)) {
				return img;
			}

			for (int i = 0; i < patches.size(); ++i) {
					
				int predLabelID = patches[i]->label()->predicted().id();
				int trueLabelID = patches[i]->label()->trueLabel().id();

				if (predLabelID == trueLabelID) {
					painter.setBrush(ColorManager::green(0.5));
					painter.setPen(ColorManager::green(0.5));
				}
				else {
					painter.setBrush(ColorManager::red(0.5));
					painter.setPen(ColorManager::red(0.5));
				}

				painter.drawPolygon(patches[i]->polygon().polygon());
			}

			return Image::qImage2Mat(outputImg);
		}

		if (options & draw_gt) {

			for (int i = 0; i < patches.size(); ++i) {
				int trueLabelID = patches[i]->label()->trueLabel().id();
				QColor trueLabelColor = ColorManager::getColor(trueLabelID, 0.5);

				painter.setBrush(trueLabelColor);
				painter.setPen(trueLabelColor);
				painter.drawPolygon(patches[i]->polygon().polygon());
				
				//trueLabelColor.setAlpha(255);
				//QPen pen = QPen(trueLabelColor);
				//pen.setWidth(5);
				//painter.setPen(pen);
				//Rect r = Rect::fromPoints(patches[i]->polygon().toPoints());
				//painter.drawRect(r.toQRect());
			}

			cv::Mat outputImgCV = Image::qImage2Mat(outputImg);
			return outputImgCV;
		}

		return draw(img);

	}

	QString FontStyleClassification::toString() const {
		return Module::toString();
	}

	bool FontStyleClassification::checkInput() const {

		if (isEmpty()) {
			qWarning() << "Missing input data for font style classification.";
			return false;
		}
		if (mClassifier->isEmpty() || !mClassifier->isTrained()) {
			qWarning() << "Font Style Classifier empty or not trained.";
			qInfo() << "Make sure font style classifier is set correctly.";
			return false;
		}
		
		return true;
	}
}