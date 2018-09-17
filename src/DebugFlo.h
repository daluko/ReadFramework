/*******************************************************************************************************
 ReadFramework is the basis for modules developed at CVL/TU Wien for the EU project READ. 
  
 Copyright (C) 2016 Markus Diem <diem@cvl.tuwien.ac.at>
 Copyright (C) 2016 Stefan Fiel <fiel@cvl.tuwien.ac.at>
 Copyright (C) 2016 Florian Kleber <kleber@cvl.tuwien.ac.at>

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
 [1] https://cvl.tuwien.ac.at/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] https://nomacs.org
 *******************************************************************************************************/

#pragma once

#include "DebugUtils.h"
#include "FormAnalysis.h"
#include "BaseModule.h"

#pragma warning(push, 0)	// no warnings from includes
 // Qt Includes
#include <QDebug>
//#include <QDir>
//#include <QImage>
//#include <QFileInfo>
#include <opencv2/core.hpp>

#pragma warning(pop)

// TODO: add DllExport magic

// Qt defines

namespace rdf {

class PageXmlParser;

// read defines
class BinarizationTest {

public:
	BinarizationTest(const DebugConfig& config = DebugConfig());

	void binarizeTest();

protected:
	DebugConfig mConfig;

};



// read defines
class TableProcessing {

public:
	TableProcessing(const DebugConfig& config = DebugConfig());

	bool match() const;
	void setTableConfig(const rdf::FormFeaturesConfig& tableConfig);

protected:
	DebugConfig mConfig;
	rdf::FormFeaturesConfig mFormConfig;

	bool load(cv::Mat& img) const;
	bool load(rdf::PageXmlParser& parser) const;

};

class LineProcessing {
public:
	LineProcessing(const DebugConfig& config = DebugConfig());
	bool lineTrace();


protected:
	bool mEstimateSkew = false;
	bool mPreFilter = true;
	cv::Mat mSrcImg = cv::Mat();
	cv::Mat mBwImg = cv::Mat();
	cv::Mat mMask = cv::Mat();
	double mPageAngle = 0;
	int preFilterArea = 10;
	QVector<rdf::Line> mLines;
	//QVector<rdf::Line> mHorLines;
	//QVector<int> mUsedHorLineIdx;
	//QVector<rdf::Line> mVerLines;
	//QVector<int> mUsedVerLineIdx;
	DebugConfig mConfig;

	bool load(cv::Mat& img) const;
	bool load(rdf::PageXmlParser& parser) const;
	bool computeBinaryInput();

};

}
