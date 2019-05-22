#include "FontStyleClassification.h"
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

#include "FontStyleClassification.h"

#include "Image.h"
#include "Utils.h"
#include "Elements.h"
#include "WhiteSpaceAnalysis.h"
#include "ImageProcessor.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QPainter>
#include "opencv2/imgproc.hpp"

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

	TextPatch::TextPatch(const cv::Mat textImg, const QString& id) : BaseElement(id) {

		mTextPatch = textImg;

		if (!generatePatchTexture()) {
			qWarning() << "Failed to generate texture from text patch.";
		}

		mLabel->setTrueLabel(LabelInfo(1, "unknown_font"));
	}

	TextPatch::TextPatch(QString text, const LabelInfo label, const QString& id) : BaseElement(id) {

		QFont font = FontStyleClassification::labelNameToFont(label.name());

		generateTextImage(text, font);
		mLabel->setTrueLabel(label);

		if (!generatePatchTexture())
			qWarning() << "Failed to generate texture from text patch.";
	}

	bool TextPatch::isEmpty() const {
		if(!mTextPatch.empty())
			return false;
		else
			return true;
	}

	int TextPatch::textureSize() const {
		return mTextureSize;
	}

	int TextPatch::textureLineHeight() const {
		return mTextureLineHeight;
	}

	void TextPatch::setTextureSize(int textureSize) {
		mTextureSize = textureSize;
	}

	void TextPatch::setTextureLineHeight(int textureLineHeight) {
		mTextureLineHeight = textureLineHeight;
	}

	bool TextPatch::generatePatchTexture() {

		if (mTextPatch.empty()) {
			qWarning() << "Couldn't create patch texture -> text patch is empty";
			mPatchTexture = cv::Mat();
			return false;
		}

		if (mTextureLineHeight > mTextureSize) {
			qWarning() << "Couldn't create patch texture -> lineHeight > patchSize";
			mPatchTexture = cv::Mat();
			return false;
		}

		//get text input image and resize + replicate it to fill an image patch fo size sz
		double sf = mTextureLineHeight / (double)mTextPatch.size().height;
		cv::Mat patchScaled = mTextPatch.clone();
		resize(mTextPatch, patchScaled, cv::Size(), sf, sf, cv::INTER_LINEAR); //TODO test different interpolation modes

		if (patchScaled.size().width > mTextureSize) {

			cv::Mat firstPatchLine = patchScaled(cv::Range::all(), cv::Range(0, mTextureSize));
			mPatchTexture = firstPatchLine.clone();

			cv::Mat tmpImg = patchScaled(cv::Range::all(), cv::Range(mTextureSize, patchScaled.size().width));
			while (tmpImg.size().width > mTextureSize) {
				cv::Mat patchLine = tmpImg(cv::Range::all(), cv::Range(0, mTextureSize));
				cv::vconcat(mPatchTexture, patchLine, mPatchTexture);
				tmpImg = tmpImg(cv::Range::all(), cv::Range(mTextureSize, tmpImg.size().width));
			}

			if (!tmpImg.empty()) {
				cv::hconcat(tmpImg, firstPatchLine, tmpImg);
				tmpImg = tmpImg(cv::Range::all(), cv::Range(0, mTextureSize));
				cv::vconcat(mPatchTexture, tmpImg, mPatchTexture);
			}

			if (mPatchTexture.size().height > mTextureSize) {
				qWarning() << "Could not fit text into text patch. Cropping image according to patch size. Some text information might be lost.";
			}

			while (mPatchTexture.size().height < mTextureSize) {
				cv::vconcat(mPatchTexture, mPatchTexture, mPatchTexture);
			}
		}
		else {
			cv::Mat textPatchLine = patchScaled.clone();

			while (textPatchLine.size().width < mTextureSize) {
				cv::hconcat(textPatchLine, patchScaled, textPatchLine);
			}

			textPatchLine = textPatchLine(cv::Range::all(), cv::Range(0, mTextureSize));
			mPatchTexture = textPatchLine.clone();
			while (mPatchTexture.size().height < mTextureSize) {
				cv::vconcat(mPatchTexture, textPatchLine, mPatchTexture);
			}
		}

		mPatchTexture = mPatchTexture(cv::Range(0, mTextureSize), cv::Range(0, mTextureSize));

		return true;
	}

	bool TextPatch::generateTextImage(QString text, QFont font, bool cropImg) {

		if (text.isEmpty()) {
			qWarning() << "Could not generate text patch image. No input text found.";
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

	QSharedPointer<PixelLabel> TextPatch::label() const {
		return mLabel;
	}

	cv::Mat TextPatch::patchTexture() const {
		return mPatchTexture;
	}

	cv::Mat TextPatch::textPatchImg() const{
		return mTextPatch;
	}

	void TextPatch::setPolygon(const Polygon & polygon) {
		mPoly = polygon;
	}

	Polygon TextPatch::polygon() const {
		return mPoly;
	}

	// FontStyleClassifier --------------------------------------------------------------------
	FontStyleClassifier::FontStyleClassifier(const FeatureCollectionManager & fcm, const cv::Ptr<cv::ml::StatModel> model, int classifierMode) {
		mModel = model;
		mFcm = fcm;
		mClassifierMode = (ClassifierMode) classifierMode;
	}

	bool FontStyleClassifier::isEmpty() const {
		return mModel->empty() || mFcm.isEmpty();
	}

	bool FontStyleClassifier::isTrained() const{
		return mModel->isTrained();
	}

	cv::Ptr<cv::ml::StatModel> FontStyleClassifier::model() const {
		return mModel;
	}

	LabelManager FontStyleClassifier::manager() const {
		return mFcm.toLabelManager();
	}

	QVector<LabelInfo> FontStyleClassifier::classify(cv::Mat testFeat) {

		//TODO test influence of normalization (in mFcm.toCvTrainData())
		//TODO consider using additional weights
		
		if (!checkInput())
			return QVector<LabelInfo>();

		cv::Mat cFeatures = testFeat;
		cFeatures.convertTo(cFeatures, CV_32FC1);

		float rawLabel = 0;
		QVector<LabelInfo> labelInfos;
		LabelManager labelManager = mFcm.toLabelManager();

		cv::Mat featStdDev;
		QVector<cv::Mat> cCentroids;
		if (mClassifierMode == ClassifierMode::classify_nn || mClassifierMode == ClassifierMode::classify_nn_wed) {
			cCentroids = mFcm.collectionCentroids();
			featStdDev = mFcm.featureSTD();
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
				else if ( mClassifierMode == ClassifierMode::classify_nn_wed) {

					//compute weighted euclidean distance by normalizing features by their standard deviation
					cv::divide(cr, featStdDev, cr);

					rawLabel = kNearest()->predict(cr);
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
		mFcm.toLabelManager().draw(p);

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

		// write features
		QJsonObject jo = mFcm.toJson(filePath);

		// write classifier model
		toJson(jo);

		int64 bw = Utils::writeJson(filePath, jo);

		return bw > 0;	// if we wrote more than 0 bytes, it's ok
	}

	void FontStyleClassifier::toJson(QJsonObject& jo) const {

		if (!mModel) {
			qWarning() << "cannot save FontStyleClassifier because statModel is NULL.";
			return;
		}

		cv::FileStorage fs(".xml", cv::FileStorage::WRITE | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_XML);
		mModel->write(fs);
#if CV_MAJOR_VERSION == 3 && CV_MINOR_VERSION == 1
		fs << "format" << 3;	// fixes bug #4402
#endif
		std::string data = fs.releaseAndGetString();

		QByteArray ba(data.c_str(), (int)data.length());
		QString ba64Str = ba.toBase64();

		jo.insert("FontStyleClassifier", ba64Str);

		jo.insert("ClassifierMode", QJsonValue(mClassifierMode));
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
		fsc->mFcm = fcm;

		if (jo.contains("ClassifierMode")) {
			fsc->mClassifierMode = (ClassifierMode) jo.value("ClassifierMode").toInt();
		}
		else
			fsc->mClassifierMode = (ClassifierMode) -1;

		fsc->mModel = FontStyleClassifier::readStatModel(jo, fsc->mClassifierMode);

		if (!fsc->mFcm.isEmpty() && !fsc->mModel->empty()) {
			qInfo() << "Font style classifier loaded from" << filePath << "in" << dt;
		}
		else {
			qCritical() << "Failed to load font style classifier from" << filePath;
			return QSharedPointer<FontStyleClassifier>::create();
		}

		return fsc;
	}

	cv::Ptr<cv::ml::StatModel> FontStyleClassifier::readStatModel(QJsonObject & jo, ClassifierMode mode) {

		// decode data
		QByteArray ba = jo.value("FontStyleClassifier").toVariant().toByteArray();
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
			qCritical() << "Can not read font style classifier from file";
			return cv::Ptr<cv::ml::StatModel>();
		}
		
		cv::Ptr<cv::ml::StatModel> model;
		if(mode == FontStyleClassifier::classify_bayes)
			model = cv::Algorithm::read<cv::ml::NormalBayesClassifier>(root);
		else if(mode == FontStyleClassifier::classify_svm)
			model = cv::Algorithm::read<cv::ml::SVM>(root);
		else if(mode == classify_knn || mode == classify_nn || mode == classify_nn_wed)
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

		if (mProcessLines) {
			cv::Mat img = mImg.clone();

			if (mGfb.isEmpty())
				mGfb = createGaborKernels();

			//debug
			QImage patchResults = Image::mat2QImage(img, true);
			QPainter painter(&patchResults);

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
			if (mGfb.isEmpty())
				mGfb = createGaborKernels();
			
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
		//use connected component information to improve gap detection
		//	strengthen regions representing one connected component

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
			data.push_back(cv::Point2f(ws.size(), 0));

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
			mFCM_test = generateFCM(features);	//do not pass additional patches with GT labels (if availabel)
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

		QString labelName = "fsl_";
		labelName += font.family() + "_";

		if (font.bold())
			labelName += "b_";
		else
			labelName += "!b_";

		if (font.italic())
			labelName += "i";
		else
			labelName += "!i";

		return labelName;
	}

	QFont FontStyleClassification::labelNameToFont(QString labelName){
		
		QStringList lp = labelName.split("_");
		if (lp.first() != "fsl" || lp.size()!= 4) {
			qWarning() << "Failed to create font from label name: " << labelName;
			return QFont();
		}
		
		QFont font;
		font.setPixelSize(30);
		font.setFamily(lp[1]);
		
		if(lp[2] == "b")
			font.setBold(true);

		if (lp[2] == "!b")
			font.setBold(false);

		if (lp[3] == "i")
			font.setItalic(true);

		if (lp[3] == "!i")
			font.setItalic(false);

		return font;
	}

	GaborFilterBank FontStyleClassification::createGaborKernels(QVector<double> theta, QVector<double> lambda, bool openCV) {

		//QVector<double> lambda = { 2 * sqrt(2), 4 * sqrt(2), 8 * sqrt(2), 16 * sqrt(2), 32 * sqrt(2), 64 * sqrt(2) };		//frequency/wavelength
		//QVector<double> mLambda = { 2 * sqrt(2), 4 * sqrt(2), 8 * sqrt(2), 16 * sqrt(2), 32 * sqrt(2) };					//frequency/wavelength
		//QVector<double> mTheta = { 0 * DK_DEG2RAD, 45 * DK_DEG2RAD, 90 * DK_DEG2RAD, 135 * DK_DEG2RAD };					//orientation

		if (lambda.isEmpty())
			lambda = { 2, 4, 8, 16, 32};					//frequency/wavelength
		
		if (theta.isEmpty())
			theta = { 0, 45, 90, 135};						//orientation
			
		for (int i = 0; i < lambda.size(); i++)
			lambda[i] *= sqrt(2);

		for (int i = 0; i < theta.size(); i++)
			theta[i] *= DK_DEG2RAD;

		//qDebug() << "lambda = " << lambda;
		//qDebug() << "theta = " << theta;

		//constant parameters			
		int ksize = 128;			//alternatives: 2^x e.g. 64, 256
		double sigma = -1;			//dependent on lambda; alternatives: 1.0; 2.0;
		double gamma = 1.0;			//alternatives: 0.5/1.0
		double psi = 0.0;			//alternatives: CV_PI * 0.5 (for real and imaginary part of gabor kernel)

		GaborFilterBank filterBank = GaborFiltering::createGaborFilterBank(lambda, theta, ksize, sigma, psi, gamma, openCV);

		return filterBank;
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

		cv::Mat predlabelMap(bbox.height(), bbox.width(), CV_8UC1, cv::Scalar(0));
		for (auto tp : mTextPatches) {
			auto poly = tp->polygon();

			if (poly.isEmpty())
				continue;
			
			cv::Mat polyImg(bbox.height(), bbox.width(), CV_8UC1, cv::Scalar(0));
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
				int trueLabelID = patches[i]->label()->trueLabel().id();
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