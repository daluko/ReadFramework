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
		
		if (!generatePatchTexture()) {
			qWarning() << "Failed to generate texture from text patch.";
		}
	}

	TextPatch::TextPatch(const cv::Mat textImg, const QString& id) : BaseElement(id) {

		mTextPatch = textImg;

		if (!generatePatchTexture()) {
			qWarning() << "Failed to generate texture from text patch.";
		}
	}

	TextPatch::TextPatch(QString text, const LabelInfo label, const QString& id) : BaseElement(id) {

		QFont font = FontStyleClassification::labelNameToFont(label.name());
		font.setPointSize(30);

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

		//TODO test/consider padding initial image
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

		//TODO check if font is valid

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

	// FontStyleClassifier --------------------------------------------------------------------
	FontStyleClassifier::FontStyleClassifier(const FeatureCollectionManager & fcm, const cv::Ptr<cv::ml::StatModel>& model, int classifierMode) {
		mModel = model;
		mFcm = fcm;
		mClassifierMode = (ClassifierMode) classifierMode;
	}

	bool FontStyleClassifier::isEmpty() const {
		return mModel->empty() || mFcm.isEmpty();
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
					qCritical() << "Unable to performe font style classification. Classifier mode is unknown.";
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

		if (!isEmpty() && !mModel->isTrained())
			qCritical() << "I cannot classify, since the model is not trained";

		return !isEmpty() && mModel->isTrained();
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
			qCritical() << "Can not load classifier from file.";
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
			qCritical() << "Could not load font style classifier from" << filePath;
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
		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();
	}

	FontStyleClassification::FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines) {
		mImg = img;
		mTextLines = textLines;
		mProcessLines = true;

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();

		mScaleFactory = QSharedPointer<ScaleFactory>(new ScaleFactory(img.size()));
	}

	FontStyleClassification::FontStyleClassification(const QVector<QSharedPointer<TextPatch>>& textPatches, QString featureFilePath) {
		mProcessLines = false;
		mTextPatches = textPatches;
		mFeatureFilePath = featureFilePath;

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();
	}

	bool FontStyleClassification::isEmpty() const {
		return mImg.empty();
	}

	bool FontStyleClassification::compute() {
		//TODO preprocessing for "real" text page input (non synthetic data)

		if (!checkInput())
			return false;

		if (mProcessLines) {
			cv::Mat img = mImg.clone();

			////rotate text lines according to baseline orientation and crop its image
			//for (auto tl : mTextLines) {
			//	//mask out text line region
			//	auto points = tl->polygon().toPoints();
			//	std::vector<cv::Point> poly;
			//	for (auto p : points) {
			//		poly.push_back(p.toCvPoint());
			//	}
			//	//cv::getRectSubPix(image, patch_size, center, patch);
			//	cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
			//	cv::fillConvexPoly(mask, poly, cv::Scalar(255, 255, 255), 16, 0);
			//	cv::Mat polyImg;
			//	img.copyTo(polyImg, mask);

			//	//rotate text line patch according to baseline angle
			//	Line baseline(Polygon(tl->baseLine().toPolygon()));
			//	double angle = baseline.angle() * (180.0 / CV_PI);
			//	//qDebug() << "line angle = " << QString::number(angle);
			//	cv::Mat rot_mat = cv::getRotationMatrix2D(baseline.center().toCvPoint(), angle, 1);

			//	cv::Mat polyRotImg;
			//	cv::warpAffine(polyImg, polyRotImg, rot_mat, polyImg.size());

			//	//processTextLine();
			//}

			////create gabor kernels
			//GaborFilterBank filterBank = createGaborKernels(false);

			////apply gabor filter bank to input image
			////cv::bitwise_not(img_gray, img_gray);
			//GaborFiltering::extractGaborFeatures(img, filterBank);
		}
		else {

			if (mTextPatches.isEmpty())
				return false;

			//get test features
			if (!mFeatureFilePath.isEmpty()) {
				mFCM_test = FeatureCollectionManager::read(mFeatureFilePath);
			}
			else {
				auto gfb = createGaborKernels();
				cv::Mat features = computeGaborFeatures(mTextPatches, gfb);
				mFCM_test = generateFCM(mTextPatches, features);
			}

			if (mFCM_test.isEmpty()) {
				return false;
			}

			cv::Mat testFeatures_, testFeatures;
			testFeatures_ = mFCM_test.toCvTrainData(-1, false)->getSamples(); //uses additional normalization
			testFeatures_.convertTo(testFeatures, CV_64F);

			//compute classification results for test features
			if (mClassifier->isEmpty()) {
				return false;
			}
			
			QVector<LabelInfo> cLabels =  mClassifier->classify(testFeatures);
			
			if (mTextPatches.size() != cLabels.size()) {
				qCritical() << "Failed to classify text patches.";
				qInfo() << "Number of test samples is out of sync with number of result labels.";
				return false;
			}

			for (int idx = 0; idx < mTextPatches.size(); idx++) {
				auto label = mTextPatches[idx]->label();
				label->setLabel(cLabels[idx]);
			}
		}

		return true;
	}

	void FontStyleClassification::setClassifier(const QSharedPointer<FontStyleClassifier>& classifier){
		mClassifier = classifier;
	}

	QVector<cv::Mat> FontStyleClassification::generateSyntheticTextPatches(QFont font, QStringList textVec) {

		QVector<cv::Mat> trainPatches;
		for (QString text : textVec) {

			cv::Mat textImg = generateSyntheticTextPatch(font, text);

			if (textImg.empty())
				continue;

			trainPatches << textImg;
		}

		return trainPatches;
	}

	cv::Mat FontStyleClassification::generateSyntheticTextPatch(QFont font, QString text) {
	
		cv::Mat textImg = generateTextImage(text, font);
		textImg = generateTextPatch(128, 30, textImg);

		if (textImg.empty())
			qWarning() << "Failed to generate text patch.";

		return textImg;
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

	cv::Mat FontStyleClassification::generateTextImage(QString text, QFont font, QRect bbox, bool cropImg) {

		QImage qImg(1, 1, QImage::Format_ARGB32);	//used only for estimating bb size
		QPainter painter(&qImg);
		painter.setFont(font);
		
		QRect tbb;
		if (bbox.isEmpty())
			tbb = painter.boundingRect(QRect(0, 0, 1, 1), Qt::AlignTop | Qt::AlignLeft, text);
		else
			tbb = bbox;
		
		painter.end();

		//reset painter device to optimized size
		qImg = QImage(tbb.size(), QImage::Format_ARGB32);
		qImg.fill(QColor(255, 255, 255, 255)); //white background
		painter.begin(&qImg);

		painter.setFont(font);
		if(bbox.isEmpty())
			painter.drawText(tbb, Qt::AlignTop | Qt::AlignLeft, text);
		else
			painter.drawText(tbb, Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, text, &bbox);

		cv::Mat img = Image::qImage2Mat(qImg);

		//crop image - ignoring white border regions
		if (cropImg) {
			cv::Mat img_gray(img.size(), CV_8UC1);
			cv::cvtColor(img, img_gray, CV_BGR2GRAY);

			cv::Mat points;
			cv::findNonZero(img_gray == 0, points);
			img = img(cv::boundingRect(points));
		}

		return img;
	}

	cv::Mat FontStyleClassification::generateTextPatch(int patchSize, int lineHeight, cv::Mat textImg) {

		cv::Mat textPatch;

		if (lineHeight > patchSize) {
			qWarning() << "Couldn't create text patch -> lineHeight > patchSize";
			return cv::Mat();
		}

		//get text input image and resize + replicate it to fill an image patch fo size sz
		double sf = lineHeight / (double)textImg.size().height;
		resize(textImg, textImg, cv::Size(), sf, sf, cv::INTER_LINEAR); //TODO test different interpolation modes

		//TODO test/consider padding initial image
		if (textImg.size().width > patchSize) {

			cv::Mat firstPatchLine = textImg(cv::Range::all(), cv::Range(0, patchSize));
			textPatch = firstPatchLine.clone();

			cv::Mat tmpImg = textImg(cv::Range::all(), cv::Range(patchSize, textImg.size().width));
			while (tmpImg.size().width > patchSize) {
				cv::Mat patchLine = tmpImg(cv::Range::all(), cv::Range(0, patchSize));
				cv::vconcat(textPatch, patchLine, textPatch);
				tmpImg = tmpImg(cv::Range::all(), cv::Range(patchSize, tmpImg.size().width));
			}

			if (!tmpImg.empty()) {
				cv::hconcat(tmpImg, firstPatchLine, tmpImg);
				tmpImg = tmpImg(cv::Range::all(), cv::Range(0, patchSize));
				cv::vconcat(textPatch, tmpImg, textPatch);
			}

			if (textPatch.size().height > patchSize) {
				qWarning() << "Could not fit text into text patch. Cropping image according to patch size. Some text information might be lost.";
			}

			while (textPatch.size().height < patchSize) {
				cv::vconcat(textPatch, textPatch, textPatch);
			}
		}
		else {
			cv::Mat textPatchLine = textImg.clone();

			while (textPatchLine.size().width < patchSize) {
				cv::hconcat(textPatchLine, textImg, textPatchLine);
			}

			textPatchLine = textPatchLine(cv::Range::all(), cv::Range(0, patchSize));
			textPatch = textPatchLine.clone();
			while (textPatch.size().height < patchSize) {
				cv::vconcat(textPatch, textPatchLine, textPatch);
			}
		}

		textPatch = textPatch(cv::Range(0, patchSize), cv::Range(0, patchSize));

		return textPatch;
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

	QSharedPointer<FontStyleClassificationConfig> FontStyleClassification::config() const {
		return qSharedPointerDynamicCast<FontStyleClassificationConfig>(mConfig);
	}

	cv::Mat FontStyleClassification::draw(const cv::Mat & img, const QColor& col) const {

		QImage qImg = Image::mat2QImage(img, true);
		QPainter painter(&qImg);

		for (auto tl : mTextLines) {

			//draw polygon
			tl->polygon().draw(painter);

			//draw baseline with skew angle
			painter.setPen(ColorManager::blue());
			Line baseline(Polygon(tl->baseLine().toPolygon()));
			Rect bbox = Rect::fromPoints(tl->polygon().toPoints());
			double angle_ = baseline.angle() * (180.0 / CV_PI);

			baseline.draw(painter);
			painter.drawText(bbox.bottomRight().toQPoint(), QString::number(angle_));
		}

		return Image::qImage2Mat(qImg);
	}

	QString FontStyleClassification::toString() const {
		return Module::toString();
	}

	bool FontStyleClassification::checkInput() const {
		return (!isEmpty() && !mTextLines.isEmpty()) || !mProcessLines;
	}
}