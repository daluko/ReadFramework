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

#pragma warning(push, 0)	// no warnings from includes
#include <QPainter>
#include "opencv2/imgproc.hpp"
#pragma warning(pop)

namespace rdf {


	// LayoutAnalysisConfig --------------------------------------------------------------------
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

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();

		mScaleFactory = QSharedPointer<ScaleFactory>(new ScaleFactory(img.size()));
	}

	bool FontStyleClassification::isEmpty() const {
		return mImg.empty() || mTextLines.isEmpty();
	}

	bool FontStyleClassification::compute() {

		//TODO implement font style classifier class for more generalized evaluation
		//TODO test performance of different classifier types
		//TODO preprocessing for "real" text page input (non synthetic data)

		if (!checkInput())
			return false;

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

		return true;
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

	QVector<int> FontStyleClassification::classifyTestWords(QVector<cv::Mat> trainSetM, cv::Mat testFeat, ClassifierMode mode){
		
		//compute mean and standard deviation
		QVector<cv::Mat> trainFeat;
		for (cv::Mat set : trainSetM) {
			cv::Mat featMat;
			for (int i = 0; i < set.size().height; ++i) {
				cv::Scalar mean_, stddev_;
				meanStdDev(set.row(i), mean_, stddev_);

				double mean = mean_[0];
				double stddev = stddev_[0];

				cv::Mat row = (cv::Mat_<double>(1, 2) << mean, stddev);
				featMat.push_back(row);
			}
			trainFeat << featMat;
		}

		//compute distances to train classes for each test sample
		QVector<int> classLabels;
		for (int i = 0; i < testFeat.size().width; ++i) {
			cv::Mat testVec = testFeat.col(i);
			int classIdx = -1;
			double minDist = DBL_MAX;
			
			for (int j = 0; j < trainFeat.size(); ++j) {
				//cv::Mat trainFeat_ = trainFeat[j];
				double dist = DBL_MAX;
				auto trainVecM = trainFeat[j].col(0);
				auto trainVecSD = trainFeat[j].col(1);

				//euclidean distance
				if(mode == ClassifierMode::classify__nn)
					dist = norm(trainVecM, testVec, cv::NORM_L2);

				if (mode == ClassifierMode::classify__nn_wed) {
					//weighted euclidean distance
					cv::Mat tmp, tmp2;
					tmp = (testVec - trainVecM);
					cv::pow(tmp, 2, tmp);
					cv::pow(trainVecSD, 2, tmp2);
					cv::divide(tmp, tmp2, tmp);
					dist = cv::sum(tmp)[0];
				}

				if (dist < minDist) {
					minDist = dist;
					classIdx = j;
				}
			}

			classLabels << classIdx;
			//qDebug() << "test word #" << i << " classified as train class " << classIdx;
		}

		return classLabels;
	}

	GaborFilterBank FontStyleClassification::createGaborKernels(bool openCV) const{

		//QVector<double> lambda = { 2 * sqrt(2), 4 * sqrt(2), 8 * sqrt(2), 16 * sqrt(2), 32 * sqrt(2), 64 * sqrt(2) };	//frequency/wavelength
		QVector<double> lambda = { 2 * sqrt(2), 4 * sqrt(2), 8 * sqrt(2), 16 * sqrt(2), 32 * sqrt(2)};	//frequency/wavelength
		QVector<double> theta = { 0 * DK_DEG2RAD, 45 * DK_DEG2RAD, 90 * DK_DEG2RAD, 135 * DK_DEG2RAD };					//orientation

		//constant parameters			
		int ksize = 128;			//alternatives: 2^x e.g. 64, 256
		double sigma = -1;			//dependent on lambda; alternatives: 1.0; 2.0;
		double gamma = 1.0;			//alternatives: 0.5/1.0
		double psi = 0.0;			//alternatives: CV_PI * 0.5 (for real and imaginary part of gabor kernel)

		GaborFilterBank filterBank = GaborFiltering::createGaborFilterBank(lambda, theta, ksize, sigma, psi, gamma, openCV);

		return filterBank;
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
		return !isEmpty();
	}
}