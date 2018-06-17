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

	// FontStyleClassification --------------------------------------------------------------------
	TextHeightEstimation::TextHeightEstimation(const cv::Mat& img) {

		mImg = img;

		// initialize scale factory
		ScaleFactory::instance().init(img.size());

		mConfig = QSharedPointer<TextHeightEstimationConfig>::create();
		mConfig->loadSettings();
	}

	bool TextHeightEstimation::isEmpty() const {
		return mImg.empty();
	}

	bool TextHeightEstimation::compute() {

		if (!checkInput())
			return false;

		cv::Mat img = mImg;

		return true;
	}

	QSharedPointer<TextHeightEstimationConfig> TextHeightEstimation::config() const {
		return qSharedPointerDynamicCast<TextHeightEstimationConfig>(mConfig);
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