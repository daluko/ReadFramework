#include "Module\FontStyleClassification.h"
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
#include "TextHeightEstimation.h"
#include "FontStyleClassification.h"
#include "PageParser.h"
#include "Elements.h"
#include "ElementsHelper.h"
#include "SuperPixelScaleSpace.h"
#include "ScaleFactory.h"

//opencv includes
#include <opencv2/imgproc.hpp>

#pragma warning(push, 0)	// no warnings from includes
#include <QDebug>
#include <QImage>
#include <QDir>
#include <QFileInfo>
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

FontStyleClassificationTest::FontStyleClassificationTest(const DebugConfig & config) {
	mConfig = config;
}

void FontStyleClassificationTest::run() {

	qDebug() << "Running font style classification test...";

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

void FontStyleClassificationTest::processDirectory(const QString dirPath){

	QDir dir(dirPath);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist!";
		return;
	}

	qInfo() << "Running font style classification test on all .tif images in directory: ";
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

void FontStyleClassificationTest::testSyntheticDataSet(QString filePath){

	FontStyleClassification fsc = FontStyleClassification();

	GaborFilterBank gfb = fsc.createGaborKernels(true);
	//GaborFilterBank gfb = fsc.createGaborKernels(false);

	//visualize gfb 
	QVector<cv::Mat> gfbImg = gfb.draw();
	cv::Mat gfbImg_1 = gfbImg[0];
	cv::Mat gfbImg_2 = gfbImg[1];

	QStringList trainWords, testWords;
	trainWords = loadTrainData(filePath);
	qInfo() << "Loaded " << trainWords.size() << " words for training the font style classifier.";
	//qDebug() << "trainingWords: " << trainWords;

	if (trainWords.size() >= 250) {
		testWords = trainWords.mid(199, 50);
		trainWords = trainWords.mid(0, 200);
	}

	QVector<cv::Mat> trainSets = generateTrainingFeatures(gfb, trainWords);

	bool italic = false;
	int weight = QFont::Normal;
	int size = 30;
	QString fFamily = "Arial";
	QFont testFont(fFamily, size, weight, italic);

	//generate test words
	if (testWords.isEmpty()) {
		testWords.append( { "Experiments", "performance", "nachrichten", "Anschlag",
		"ablehnen", "verstehen" , "done" , "test" , "global", "patch", "Zeit", "Bild", "Hotel" });
	}

	qInfo() << "Using " << testWords.size() << " words for testing the font style classifier.";

	//compute font style test 0 -------------------------------------------------------------------------------------------------
	cv::Mat testData = generateTestFeatures(testWords, testFont, gfb);
	QVector<int> classLabels0 = fsc.classifyTestWords(trainSets, testData, FontStyleClassification::classify__nn_wed);

	double precision0 = computePrecision(classLabels0, 0);
	qInfo() << "Precision for !bold + !italic = " << precision0;

	//compute font style test 1 -------------------------------------------------------------------------------------------------
	testFont.setBold(true);
	testFont.setItalic(false);

	testData = generateTestFeatures(testWords, testFont, gfb);
	QVector<int> classLabels1 = fsc.classifyTestWords(trainSets, testData, FontStyleClassification::classify__nn_wed);

	double precision1 = computePrecision(classLabels1, 1);
	qInfo() << "Precision for bold + !italic = " << precision1;
	
	//compute font style test 2 -------------------------------------------------------------------------------------------------
	testFont.setBold(true);
	testFont.setItalic(true);

	testData = generateTestFeatures(testWords, testFont, gfb);
	QVector<int> classLabels2 = fsc.classifyTestWords(trainSets, testData, FontStyleClassification::classify__nn_wed);

	double precision2 = computePrecision(classLabels2, 2);
	qInfo() << "Precision for bold + italic = " << precision2;
	
	//compute font style test 3 -------------------------------------------------------------------------------------------------
	testFont.setBold(false);
	testFont.setItalic(true);

	testData = generateTestFeatures(testWords, testFont, gfb);
	QVector<int> classLabels3 = fsc.classifyTestWords(trainSets, testData, FontStyleClassification::classify__nn_wed);

	double precision3 = computePrecision(classLabels3, 3);
	qInfo() << "Precision for !bold + italic = " << precision3;

	//overall classification precision ------------------------------------------------------------------------------------------
	double precision = (precision0 + precision1 + precision2 + precision3) / 4.0;
	qInfo() << "Overall font style classification precision = " << precision;


	//QString testFilePath = "F:/dev/da/CVL/ReadFrameworkDaluko/ReadFramework/resources/FontTrainData.txt";
	//generateSnytheticTestPage(testFilePath);

	qInfo() << "Finished font style classification test on synthetic data.";
}

QVector<cv::Mat> FontStyleClassificationTest::generateTrainingFeatures(GaborFilterBank gfb, QStringList trainWords){

	bool italic = false;
	int weight = QFont::Normal;
	int size = 30;
	QString fFamily = "Arial";
	QFont font(fFamily, size, weight, italic);

	//create test image
	QVector<cv::Mat> trainPatches1 = FontStyleClassification::generateSyntheticTextPatches(font, trainWords);
	//cv::Mat sample1 = trainPatches1[0];

	font.setBold(true);
	QVector<cv::Mat> trainPatches2 = FontStyleClassification::generateSyntheticTextPatches(font, trainWords);
	//cv::Mat sample2 = trainPatches2[0];

	font.setItalic(true);
	QVector<cv::Mat> trainPatches3 = FontStyleClassification::generateSyntheticTextPatches(font, trainWords);
	//cv::Mat sample3 = trainPatches3[0];

	font.setBold(false);
	font.setItalic(true);
	QVector<cv::Mat> trainPatches4 = FontStyleClassification::generateSyntheticTextPatches(font, trainWords);
	//cv::Mat sample4 = trainPatches4[0];

	QVector<QVector<cv::Mat>> trainingSets = { trainPatches1, trainPatches2, trainPatches3, trainPatches4 };
	//QVector<QVector<cv::Mat>> trainingSets = { trainPatches1, trainPatches3};

	//compute gabor features
	QVector<cv::Mat> featMatManager;
	for (auto set : trainingSets) {
		cv::Mat meanFeatVec, featMat;
		for (cv::Mat patch : set) {
			cv::Mat features = GaborFiltering::extractGaborFeatures(patch, gfb);
			if (!featMat.empty())
				cv::hconcat(featMat, features, featMat);
			else
				featMat = features.clone();
		}
		featMatManager << featMat;
	}

	//return featMatManager;
	return featMatManager;
}

cv::Mat FontStyleClassificationTest::generateTestFeatures(QStringList testWords, QFont font, GaborFilterBank gfb) {

	std::sort(testWords.begin(), testWords.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.size() > rhs.size();
	});

	QVector<cv::Mat> testPatches = FontStyleClassification::generateSyntheticTextPatches(font, testWords);
	cv::Mat testSample = testPatches[0];

	cv::Mat testFeatMat;
	for (cv::Mat patch : testPatches) {
		cv::Mat features = GaborFiltering::extractGaborFeatures(patch, gfb);

		if (!testFeatMat.empty())
			cv::hconcat(testFeatMat, features, testFeatMat);
		else
			testFeatMat = features.clone();
	}

	return testFeatMat;
}

void FontStyleClassificationTest::generateDataFromTextFile(QString filePath){

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open text file containing training data.";
		return;
	}

	QTextStream inStream(&file);
	QString inputText = inStream.readAll();
	inStream.flush();
	file.close();

	//qDebug() << inputText;

	if (inputText.isEmpty()) {
		qInfo() << "Text file is empty.";
		return;
	}

	//use alphanumeric symbols only 
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[-`~!@#$%^&*()_â€”+=|:;<>Â«Â»,.?/{}\'\"\\\[\\\]\\\\]")), " ");
	//remove numbers too (letters only)
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[0 - 9]")), " ");
	//remove formatting symbols
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[\\n\\t\\r]"))," ");
	
	qDebug() << inputText;

	QStringList wordListTmp = inputText.split(' ');

	QStringList wordList;
	for (auto word : wordListTmp){
		if (word.length() >= 4)	//use only words with 4+ characters
			wordList.append(word);
	}
	
	QString csvPath = Utils::createFilePath(filePath, "", "csv");
	QFile outFile(csvPath);

	if (outFile.open(QFile::WriteOnly | QFile::Truncate)) {
		QTextStream outStream(&outFile);
		outStream << wordList.join(",") << endl;
		outStream.flush();
		file.close();
	}
	else {
		qWarning() << "Could not save csv file for training data.";
	}
}

cv::Mat FontStyleClassificationTest::generateSnytheticTestPage(QString filePath) {

	FontStyleClassification fsc = FontStyleClassification();

	//font used for printing test page
	bool italic = false;
	int weight = QFont::Normal;
	int size = 30;
	QString fFamily = "Arial";
	QFont font(fFamily, size, weight, italic);
	
	QFile file(filePath);

	if (!file.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open text file containing test data.";
		return cv::Mat();
	}
	
	QTextStream inStream(&file);
	QString inputText = inStream.readAll();
	file.close();

	if (inputText.isEmpty()) {
		qInfo() << "Text file is empty.";
		return cv::Mat();
	}

	//cv::Mat textImg = fsc.generateTextImage(inputText, font, QRect(0, 0, 1000, 2000), false);
	cv::Mat textImg = fsc.generateTextImage(inputText, font, QRect(), false);

	return textImg;
}

double FontStyleClassificationTest::computePrecision(QVector<int> labels, int trueLabel){

	int tp = 0;
	for (int l : labels) {
		if (l == trueLabel)
			tp++;
	}

	double precision = tp / (double)labels.size();

	return precision;
}

QStringList FontStyleClassificationTest::loadTrainData(QString filePath) {

	QFile csvFile(filePath);
	if (!csvFile.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open csv file containing train data.";

		QString textFilePath = Utils::createFilePath(filePath, "", "txt");
		qInfo() << "Generating train data from text file:" << textFilePath;
		generateDataFromTextFile(textFilePath);

		if (!csvFile.open(QIODevice::ReadOnly)) {
			qWarning() << "Could neither open nor generate csv file containing train data.";
			qInfo() << "Please provide a .txt or .csv file containing words for training synthetic font style classifcation.";
			return QStringList();
		}
	}

	QStringList wordList;

	while (!csvFile.atEnd()) {
		QString line = csvFile.readLine();
		wordList.append(line.trimmed().split(','));
	}

	return wordList;
}


}