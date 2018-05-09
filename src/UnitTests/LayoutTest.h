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
 [1] http://www.cvl.tuwien.ac.at/cvl/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] http://nomacs.org
 *******************************************************************************************************/

#pragma once

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#pragma warning(pop)

#include "TestUtils.h"

// Qt defines
namespace cv {
	class Mat;
}

namespace rdf {

class PageXmlParser;

// read defines
class BaselineTest {

public:
	BaselineTest(const TestConfig& config = TestConfig());

	bool baselineTest() const;

protected:
	TestConfig mConfig;

	bool layoutToXml(const cv::Mat& img, const PageXmlParser& parser) const;
};

class SuperPixelTest {

public:
	SuperPixelTest(const TestConfig& config = TestConfig());

	bool testSuperPixel() const;
	bool collectFeatures() const;
	bool train() const;
	bool eval() const;

protected:
	TestConfig mConfig;

	bool load(cv::Mat& img) const;
	bool load(rdf::PageXmlParser& parser) const;
};


}