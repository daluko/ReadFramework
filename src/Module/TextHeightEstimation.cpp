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

#include "TextHeightEstimation.h"

#include "Image.h"
#include "Utils.h"
#include "Elements.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QPainter>
#include "opencv2/imgproc.hpp"
#pragma warning(pop)

namespace rdf {


	// LayoutAnalysisConfig --------------------------------------------------------------------
	TextHeightEstimationConfig::TextHeightEstimationConfig() : ModuleConfig("Text Height Estimation Module") {
	}

	QString TextHeightEstimationConfig::toString() const {
		return ModuleConfig::toString();
	}

	void TextHeightEstimationConfig::setTestBool(bool testBool) {
		mTestBool = testBool;
	}

	bool TextHeightEstimationConfig::testBool() const {
		return mTestBool;
	}

	void TextHeightEstimationConfig::setTestInt(int testInt) {
		mTestInt = testInt;
	}

	int TextHeightEstimationConfig::testInt() const {
		return ModuleConfig::checkParam(mTestInt, 0, INT_MAX, "testInt");
	}

	void TextHeightEstimationConfig::setTestPath(const QString & tp) {
		mTestPath = tp;
	}

	QString TextHeightEstimationConfig::testPath() const {
		return mTestPath;
	}

	void TextHeightEstimationConfig::load(const QSettings & settings) {

		mTestBool = settings.value("testBool", testBool()).toBool();
		mTestInt = settings.value("testInt", testInt()).toInt();		
		mTestPath = settings.value("classifierPath", testPath()).toString();
	}

	void TextHeightEstimationConfig::save(QSettings & settings) const {

		settings.setValue("testBool", testBool());
		settings.setValue("testInt", testInt());
		settings.setValue("testPath", testPath());
	}

	// TextHeightEstimation --------------------------------------------------------------------
	TextHeightEstimation::TextHeightEstimation(const cv::Mat& img) {

		mImg = img;

		mConfig = QSharedPointer<TextHeightEstimationConfig>::create();
		mConfig->loadSettings();

		mScaleFactory = QSharedPointer<ScaleFactory>(new ScaleFactory(img.size()));
	}

	bool TextHeightEstimation::isEmpty() const {
		return mImg.empty();
	}

	bool TextHeightEstimation::compute() {

		//TODO fix parameters

		if (!checkInput())
			return false;

		int numSplitLevels = 5;
		imagePatches = QVector<QVector<ImagePatch>>(numSplitLevels);

		cv::Mat img = mImg;
		cv::Mat img_gray;
		cv::cvtColor(img, img_gray, cv::COLOR_BGR2GRAY);

		//saves split image patches of input image in imagePatches variable
		subdivideImage(img_gray, numSplitLevels);

		//test text height estimation
		textHeightEstimation(img_gray, imagePatches[1][0]);

		return true;
	}


	QSharedPointer<TextHeightEstimationConfig> TextHeightEstimation::config() const {
		return qSharedPointerDynamicCast<TextHeightEstimationConfig>(mConfig);
	}

	void TextHeightEstimation::subdivideImage(const cv::Mat img, int numSplitLevels) {
	
		
		cv::Range r_test1(0, 20);
		QVector<cv::Range> split_ranges1 = splitRange(r_test1);

		QVector<cv::Range> split_ranges2 = splitRange(split_ranges1[0]);
		QVector<cv::Range> split_ranges3 = splitRange(split_ranges1[1]);

		QVector<cv::Range> split_ranges4 = splitRange(split_ranges3[0]);
		QVector<cv::Range> split_ranges5 = splitRange(split_ranges3[1]);

		//level 0 won't be used (contribuition very poor according to Pintus et al.)
		ImagePatch p0;
		p0.xRange = cv::Range(0, img.cols);
		p0.yRange = cv::Range(0, img.rows);
		imagePatches[0] << QVector<ImagePatch>(1, p0);

		for (int i = 1; i < numSplitLevels; i++) {
			for (auto p : imagePatches[i - 1]) {
				auto xRs = splitRange(p.xRange);
				auto yRs = splitRange(p.yRange);

				ImagePatch pi_0 = { xRs[0], yRs[0], NULL };
				ImagePatch pi_1 = { xRs[1], yRs[0], NULL };
				ImagePatch pi_2 = { xRs[0], yRs[1], NULL };
				ImagePatch pi_3 = { xRs[1], yRs[1], NULL };
				
				if (imagePatches[i].isEmpty()) {
					QVector<ImagePatch> pi;
					pi << pi_0 << pi_1 << pi_2 << pi_3;
					imagePatches[i].append(pi);
				}
				else {
					imagePatches[i] << pi_0 << pi_1 << pi_2 << pi_3;
				}
			}
		}
	}

	QVector<cv::Range> TextHeightEstimation::splitRange(cv::Range range) const {

		QVector<cv::Range> splitRanges;
		
		if ((range.size() % 2) == 0) {
			int rm = range.start + (int) (range.size() / 2);
			cv::Range r1(range.start,  +  rm);
			cv::Range r2(rm, range.end);

			splitRanges << r1 << r2;
		}
		else {
			int test = std::ceil(range.size() / 2);
			int test1 = std::ceil(range.size() / 2.0);
			int rm = range.start + std::ceil(range.size() / 2.0);
			cv::Range r1(range.start, rm);
			cv::Range r2(rm-1, range.end);

			splitRanges << r1 << r2;
		}

		return splitRanges;
	}

	void TextHeightEstimation::textHeightEstimation(const cv::Mat input_img, ImagePatch patch){

		cv::Mat img = input_img(patch.xRange, patch.yRange);
		cv::Mat dftInput, dftResult, mulSpec, dftInverse, dftInverseConv, vPP, vPPconv;

		img.convertTo(dftInput, CV_32F);

		dft(dftInput, dftResult, cv::DFT_COMPLEX_OUTPUT);
		cv::mulSpectrums(dftResult, dftResult, mulSpec, 0, true);
		dft(mulSpec, dftInverse, cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT);

		double minVal, maxVal;
		cv::minMaxLoc(dftInverse, &minVal, &maxVal); //find minimum and maximum intensities
		dftInverse.convertTo(dftInverseConv, CV_8U, 255.0 / (maxVal - minVal), -minVal * 255.0 / (maxVal - minVal));

		reduce(dftInverseConv, vPP, 1, CV_REDUCE_SUM, CV_32F);
		cv::minMaxLoc(vPP, &minVal, &maxVal); //find minimum and maximum intensities
		vPP.convertTo(vPPconv, CV_8U, 64.0 / (maxVal - minVal), -minVal * 64.0 / (maxVal - minVal));

		//draw vertical projection profile within dft image
		int max_x = dftInverseConv.cols - 1;
		for (int i = 1; i < vPP.rows; i++) {
			cv::Point p1 = cv::Point(max_x - vPPconv.at<uchar>(i - 1), i - 1);
			cv::Point p2 = cv::Point(max_x - vPPconv.at<uchar>(i), i);
			cv::line(dftInverseConv, p1, p2, cv::Scalar(255), 2, 8, 0);
		}
		
		qDebug() << "Text height estimation";
	}

	cv::Mat TextHeightEstimation::draw(const cv::Mat & img, const QColor& col) const {

		QImage qImg = Image::mat2QImage(img, true);
		QPainter painter(&qImg);

		return Image::qImage2Mat(qImg);
	}

	QString TextHeightEstimation::toString() const {
		return Module::toString();
	}

	bool TextHeightEstimation::checkInput() const {
		return !isEmpty();
	}
}