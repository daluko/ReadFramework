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

#include "DebugDavid.h"
#include "Utils.h"
#include "Image.h"
#include "SuperPixel.h"
#include "WhiteSpaceAnalysis.h"
#include "PageParser.h"
#include "Elements.h"

#include "SuperPixelScaleSpace.h"
#include "ScaleFactory.h"

#include <QImage>
#include <QFileInfo>

#include <opencv2/imgproc.hpp>

#pragma warning(push, 0)	// no warnings from includes
#include <QDebug>
#pragma warning(pop)


namespace rdf {

WhiteSpaceTest::WhiteSpaceTest(const DebugConfig & config) {
	mConfig = config;
}

void WhiteSpaceTest::run() {
	
	qDebug() << "Running White Space Analysis test...";
	
	Timer dt;

	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (!imgCv.empty())
		qInfo() << mConfig.imagePath() << "loaded...";
	else
		qInfo() << mConfig.imagePath() << "NOT loaded...";

	WhiteSpaceAnalysis wsa(imgCv);
	wsa.compute();

	//-------------------------xml text lines
	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
	xmlPath = Utils::createFilePath(xmlPath, "-wsa_lines");
	rdf::PageXmlParser parser;
	bool xml_found = parser.read(xmlPath);

	// set xml header info
	QSharedPointer<PageElement> xmlPage = parser.page();
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(QSize(qImg.size()));
	xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	//add results to xml
	xmlPage->rootRegion()->removeAllChildren();
	for (auto tr : wsa.textLineRegions()) {
		xmlPage->rootRegion()->addChild(tr);
	}

	//write xml file	
	parser.write(xmlPath, xmlPage);


	////-------------------------xml text blocks
	xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
	xmlPath = Utils::createFilePath(xmlPath, "-wsa_block");
	xml_found = parser.read(xmlPath);

	//add results to xml
	xmlPage->rootRegion()->removeAllChildren();
	xmlPage->rootRegion()->addChild(wsa.textBlockRegions());
	//for (auto tr : wsa.textBlocks()) {
	//	xmlPage->rootRegion()->addChild(tr);
	//}
	

	parser.write(xmlPath, xmlPage);
}

}