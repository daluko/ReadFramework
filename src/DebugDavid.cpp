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
#include "FontStyleClassification.h"
#include "TextHeightEstimation.h"
#include "PageParser.h"
#include "Elements.h"
#include "ElementsHelper.h"

#include "SuperPixelScaleSpace.h"
#include "ScaleFactory.h"

#include <QImage>
#include <QFileInfo>
#include <QDir>

#include <opencv2/imgproc.hpp>

#pragma warning(push, 0)	// no warnings from includes
#include <QDebug>
#pragma warning(pop)


namespace rdf {

TextHeightEstimationTest::TextHeightEstimationTest(const DebugConfig & config) {
	mConfig = config;
}

void TextHeightEstimationTest::run() {
	
	bool debugDraw = true;

	qDebug() << "Running text height estimation test...";
	Timer dt;

	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (imgCv.empty()) {
		qInfo() << mConfig.imagePath() << "NOT loaded...";
		return;
	}

	TextHeightEstimation the(imgCv);

	if(debugDraw)
		the.config()->setDebugDraw(true);

	the.compute();

	cv::Mat img_result = the.draw(imgCv);
	QString resultVals = QString::number(the.textHeightEstimate())+ "_" + QString::number(the.confidence()) + "_" + QString::number(the.coverage()) + "_" + QString::number(the.relCoverage());
	QString imgPath = Utils::createFilePath(mConfig.imagePath(), "_the_result_" + resultVals, "png");
	Image::save(img_result, imgPath);
	
	if (debugDraw)
		the.drawDebugImages(mConfig.imagePath());

}

void TextHeightEstimationTest::processDirectory(QString dirPath) {

	QDir dir(dirPath);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist!";
		return;
	}

	qInfo() << "Running Text Height Estimation test on all .tif images in directory: ";
	qInfo() << dirPath;

	Timer dt;

	QStringList filters;
	filters << "*.tif";
	QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

	int i = 0;
	for (auto f : fileInfoList) {
		++i;
		qDebug() << "processing image #" << QString::number(i) << " : " << f.absoluteFilePath();
		mConfig.setImagePath(f.absoluteFilePath());
		run();
	}

	qInfo() << "Directory processed in " << dt;
}
	
WhiteSpaceTest::WhiteSpaceTest(const DebugConfig & config) {
	mConfig = config;
}

void WhiteSpaceTest::run() {
	
	qInfo() << "Running White Space Analysis test...";

	Timer dt;
	bool debugDraw = false;
	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (!imgCv.empty())
		qInfo() << mConfig.imagePath() << "loaded...";
	else
		qInfo() << mConfig.imagePath() << "NOT loaded...";

	WhiteSpaceAnalysis wsa(imgCv);

	if(wsa.config()->debugPath().isEmpty())
		wsa.config()->setDebugPath(mConfig.imagePath());
	
	wsa.config()->setDebugDraw(debugDraw);

	wsa.compute();

	QString xmlPath;
	rdf::PageXmlParser parser;
	bool xml_found;
	QSharedPointer<PageElement> xmlPage;

	////-------------------------xml text lines
	//xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.imagePath());
	//xmlPath = Utils::createFilePath(xmlPath, "-wsa_lines");
	//xml_found = parser.read(xmlPath);

	//// set up xml page
	//xmlPage = parser.page();
	//xmlPage->setCreator(QString("CVL"));
	//xmlPage->setImageSize(QSize(qImg.size()));
	//xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	////add results to xml
	//xmlPage = parser.page();
	//xmlPage->rootRegion()->removeAllChildren();

	//for (auto tr : wsa.textLineRegions()) {
	//	xmlPage->rootRegion()->addChild(tr);
	//}
	//parser.write(xmlPath, xmlPage);

	//////-------------------------xml text blocks
	//xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.imagePath());
	//xmlPath = Utils::createFilePath(xmlPath, "-wsa_blocks+lines");
	//xml_found = parser.read(xmlPath);

	//// set up xml page
	//xmlPage = parser.page();
	//xmlPage->setCreator(QString("CVL"));
	//xmlPage->setImageSize(QSize(qImg.size()));
	//xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	////add results to xml
	//xmlPage = parser.page();
	//xmlPage->rootRegion()->removeAllChildren();
	//xmlPage->rootRegion()->addChild(wsa.textBlockRegions());
	//parser.write(xmlPath, xmlPage);

	////-------------------------eval xml text block regions

	//NOTE: produce eval xml at the end -> text lines (children) are removed from results
	xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.imagePath());
	xml_found = parser.read(xmlPath);

	// set up xml page
	xmlPage = parser.page();
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(QSize(qImg.size()));
	xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	xmlPage->rootRegion()->removeAllChildren();
	for (auto tr : wsa.evalTextBlockRegions()) {
		xmlPage->rootRegion()->addChild(tr);
	}
	parser.write(xmlPath, xmlPage);

	qInfo() << "White space layout analysis results computed in " << dt;
}

void WhiteSpaceTest::processDirectory(QString dirPath) {

	QDir dir(dirPath);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist!";
		return;
	}

	qInfo() << "Running White Space Analysis test on all .tif images in directory: ";
	qInfo() << dirPath;

	Timer dt;

	QStringList filters;
	filters << "*.tif";
	QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

	int i = 0;
	for (auto f : fileInfoList) {
		++i;
		qDebug() << "processing image #" << QString::number(i) << " : " << f.absoluteFilePath();
		mConfig.setImagePath(f.absoluteFilePath());
		run();
	}
	
	qInfo() << "Directory processed in " << dt;
}

FontClassificationTest::FontClassificationTest(const DebugConfig & config) {
	mConfig = config;
}

void FontClassificationTest::run() {

	qDebug() << "Running font style calssification test...";

	Timer dt;

	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (imgCv.empty()) {
		qInfo() << mConfig.imagePath() << "NOT loaded...";
		return;
	}

	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
	rdf::PageXmlParser parser;
	bool xml_found = parser.read(xmlPath);

	if (!xml_found){
		qInfo() << xmlPath << "NOT found...";
		return;
	}

	QSharedPointer<PageElement> xmlPage = parser.page();
	QVector<QSharedPointer<TextLine>>textLineRegions;

	QVector<QSharedPointer<Region> > textRegions = RegionManager::filter<Region>(xmlPage->rootRegion(), Region::type_text_line);
	for (auto tr : textRegions) {
		textLineRegions << qSharedPointerCast<TextLine>(tr);
	}

	FontStyleClassification fsc(imgCv, textLineRegions);
	fsc.compute();

	cv::Mat img_result = fsc.draw(imgCv);
	QString imgPath = Utils::createFilePath(xmlPath, "_fsc_results", "png");
	Image::save(img_result, imgPath);
}

}