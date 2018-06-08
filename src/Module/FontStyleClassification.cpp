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
	FontStyleClassification::FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines) {

		mImg = img;
		mTextLines = textLines;

		// initialize scale factory
		ScaleFactory::instance().init(img.size());

		mConfig = QSharedPointer<FontStyleClassificationConfig>::create();
		mConfig->loadSettings();
	}

	bool FontStyleClassification::isEmpty() const {
		return mImg.empty();
	}

	bool FontStyleClassification::compute() {

		if (!checkInput())
			return false;

		cv::Mat img = mImg;

		for (auto tl : mTextLines) {
			//mask out text line region
			auto points = tl->polygon().toPoints();
			std::vector<cv::Point> poly;
			for (auto p : points) {
				poly.push_back(p.toCvPoint());
			}
			//cv::getRectSubPix(image, patch_size, center, patch);
			cv::Mat mask = cv::Mat::zeros(img.size(), CV_8U);
			cv::fillConvexPoly(mask, poly, cv::Scalar(255, 255, 255), 16, 0);
			cv::Mat polyImg;
			img.copyTo(polyImg, mask);

			//rotate text line patch according to baseline angle
			Line baseline(Polygon(tl->baseLine().toPolygon()));
			double angle = baseline.angle() * (180.0 / CV_PI);
			//qDebug() << "line angle = " << QString::number(angle);
			cv::Mat rot_mat = cv::getRotationMatrix2D(baseline.center().toCvPoint(), angle, 1);

			cv::Mat polyRotImg;
			cv::warpAffine(polyImg, polyRotImg, rot_mat, polyImg.size());

			//processTextLine();
		}

		return true;
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

		//QString path = "E:/data/test/HBR2013_training/debug.png";
		//QString imgPath = Utils::createFilePath(path, "_poly_img");
		//Image::save(Image::qImage2Mat(qImg), imgPath);

		return Image::qImage2Mat(qImg);
	}

	QString FontStyleClassification::toString() const {
		return Module::toString();
	}

	bool FontStyleClassification::checkInput() const {

		return !isEmpty();
	}
}