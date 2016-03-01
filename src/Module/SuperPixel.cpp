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

#include "SuperPixel.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QDebug>

#include <opencv2/stitching/detail/seam_finders.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#pragma warning(pop)

namespace rdf {


// SuperPixel --------------------------------------------------------------------
SuperPixel::SuperPixel(const cv::Mat& srcImg) {
	mSrcImg = srcImg;

	mModuleName = "SuperPixel";
	loadSettings();
}

bool SuperPixel::checkInput() const {

	if (mSrcImg.empty()) {
		mWarning << "the source image must not be empty...";
		return false;
	}

	return true;
}

bool SuperPixel::isEmpty() const {
	return mSrcImg.empty();
}

void SuperPixel::load(const QSettings& settings) {

	//mThresh = settings.value("thresh", mThresh).toInt();
}

void SuperPixel::save(QSettings& settings) const {

	// TODO
	//settings.setValue("thresh", mThresh);
}

bool SuperPixel::compute() {

	if (!checkInput())
		return false;

	cv::detail::GraphCutSeamFinder seamFinder;

	std::vector<cv::Point> corners;
	corners.push_back(cv::Point(0, 0));
	corners.push_back(cv::Point(100, 200));

	cv::UMat src;
	cv::cvtColor(mSrcImg, src, CV_RGBA2RGB);

	std::vector<cv::UMat> mats;
	mats.push_back(src);

	qDebug() << "src channels: " << mSrcImg.channels();

	seamFinder.find(mats, corners, mMask);

	qDebug() << "mMask size: " << mMask.size();

	// I guess here is a good point to save the settings
	saveSettings();
	mDebug << " computed...";

	return true;
}

cv::Mat SuperPixel::binaryImage() const {
	
	if (mMask.empty())
		return cv::Mat();
	
	cv::Mat img;
	mMask[0].copyTo(img);

	return img;
}

QString SuperPixel::toString() const {

	QString msg = debugName();

	return msg;
}

}