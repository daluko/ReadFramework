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
#include "PageParser.h"
#include "Elements.h"
#include "ElementsHelper.h"
#include "SuperPixelScaleSpace.h"
#include "ScaleFactory.h"
#include "PixelLabel.h"
#include "TextHeightEstimation.h"
#include "FontStyleTrainer.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QImage>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QFontDataBase>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextBlock>
#include <QAbstractTextDocumentLayout>
#include <QTextList>
#include <QRandomGenerator>
#pragma warning(pop)


namespace rdf {

//Text Height Estimation Test ------------------------------------------------------------------------------
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

	qInfo() << "Running Text Height Estimation test on all *.tif and *.jpg files in directory: ";
	qInfo() << dirPath;

	Timer dt;

	QStringList filters;
	filters << "*.tif" << "*.jpg";
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

bool TextHeightEstimationTest::drawTextHeightRect(QRect thr){

	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (imgCv.empty()) {
		qInfo() << mConfig.imagePath() << "NOT loaded...";
		return false;
	}
	
	qImg.convertToFormat(QImage::Format_ARGB32);

	if (thr.width() > qImg.width() || thr.height() > qImg.height())
		return false;

	//QPixmap padded(qImg.width() + 20 + thr.width(), qImg.height());
	QPixmap padded(qImg.width(), qImg.height());
	padded.fill(Qt::transparent);
	//padded.fill(QColor(0,0,0,255));

	QPainter painter(&padded);

	//QImage padded(qImg.width() + 20 + thr.width(), qImg.height(), QImage::Format_ARGB32);

	//QPainter painter(&padded);
	painter.drawImage(QPoint(0, 0), qImg);

	painter.setPen(ColorManager::blue().rgba());
	painter.drawRect(QRect(qImg.width() + 10, 10, thr.width(), thr.height()));
	int xHeight = 0;
	while ((xHeight + thr.height()) < qImg.height()) {
		painter.drawRect(QRect((int)std::round(qImg.width() / 2.0), xHeight, thr.width(), thr.height()));
		xHeight = xHeight + thr.height();
	}
	
	QString outPath = Utils::createFilePath(mConfig.imagePath(), "_text_height_rect", "png");
	//Image::save(img_debug, outPath);
	padded.save(outPath, "PNG");

	//QString outPath = Utils::createFilePath(mConfig.imagePath(), "_text_height_rect_", "png");
	//cv::Mat img_debug = Image::qImage2Mat(padded);
	//Image::save(img_debug, outPath);
	qDebug() << "Saving THE debug image to: " << outPath;

	return true;
}

//White Space Analysis Test ------------------------------------------------------------------------------
WhiteSpaceTest::WhiteSpaceTest(const DebugConfig & config) {
	mConfig = config;
	mWsaConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
	mTlhConfig = QSharedPointer<TextLineHypothesizerConfig>::create();
	mWssConfig = QSharedPointer<WhiteSpaceSegmentationConfig>::create();
	mTbfConfig = QSharedPointer<TextBlockFormationConfig>::create();
}

void WhiteSpaceTest::run() {
	
	qInfo() << "Running White Space Analysis test...";

	Timer dt;
	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);

	if (!imgCv.empty())
		qInfo() << mConfig.imagePath() << "loaded...";
	else
		qInfo() << mConfig.imagePath() << "NOT loaded...";

	//disable white space gap detection
	//mWssConfig->setFindWhiteSpaceGaps(false);

	WhiteSpaceAnalysis wsa(imgCv);
	wsa.setConfig(mWsaConfig);
	wsa.setTlhConfig(mTlhConfig);
	wsa.setWssConfig(mWssConfig);
	wsa.setTbfConfig(mTbfConfig);

	if (wsa.config()->debugPath().isEmpty()) 
		wsa.config()->setDebugPath(mConfig.outputPath());
	
	//wsa.config()->setDebugDraw(true);
	wsa.compute();

	//create ouput xml -------------------------
	QString xmlPath;
	rdf::PageXmlParser parser;
	bool xml_found;
	QSharedPointer<PageElement> xmlPage;

	//xml with text line hypotheses
	bool writeLineHypos = true;
	if (writeLineHypos) {
		xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
		//xmlPath = Utils::createFilePath(xmlPath, "-wsa_lineHypos");
		xml_found = parser.read(xmlPath);

		// set up xml page
		xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(qImg.size()));
		xmlPage->setImageFileName(QFileInfo(mConfig.imagePath()).fileName());

		//add results to xml
		xmlPage = parser.page();
		xmlPage->rootRegion()->removeAllChildren();

		auto allInTr = QSharedPointer<TextRegion>::create();
		allInTr->setId("cvl-" + allInTr->id().remove("{").remove("}"));	//avoid errors when using as input for aletheia eval tool
		QPolygonF polygon = QPolygonF();
		polygon << QPointF(0, 0) << QPointF(1, 0) << QPointF(1, 1) << QPointF(0, 1);
		allInTr->setPolygon(polygon);

		for (auto tr : wsa.textLineHypotheses()) {
			allInTr->addChild(tr);	//use one region to insert all line regions	
		}

		xmlPage->rootRegion()->addChild(allInTr);

		parser.write(xmlPath, xmlPage);
	}

	//xml with text lines
	bool writeLines = false;
	if (writeLines) {
		xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
		xmlPath = Utils::createFilePath(xmlPath, "-wsa_lines");
		parser.read(xmlPath); //creates new file if it does not exist

		// set up xml page
		xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(qImg.size()));
		xmlPage->setImageFileName(QFileInfo(mConfig.imagePath()).fileName());

		//add results to xml
		xmlPage->rootRegion()->removeAllChildren();

		for (auto tr : wsa.textLineRegions()) {
			xmlPage->rootRegion()->addChild(tr);
		}
		parser.write(xmlPath, xmlPage);
	}

	//xml with text blocks + lines
	bool writeLinesNBlocks = false;
	if (writeLinesNBlocks) {
		xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
		xmlPath = Utils::createFilePath(xmlPath, "-wsa_blocks+lines");
		xml_found = parser.read(xmlPath);

		// set up xml page
		xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(qImg.size()));
		xmlPage->setImageFileName(QFileInfo(mConfig.imagePath()).fileName());

		//add results to xml
		xmlPage = parser.page();
		xmlPage->rootRegion()->removeAllChildren();
		xmlPage->rootRegion()->addChild(wsa.textBlockRegions());
		parser.write(xmlPath, xmlPage);
	}

	//xml with eval text block regions

	bool writeEvalBlocks = false;
	if (writeEvalBlocks) {
		//NOTE: produce eval block xml at the end -> text lines (children) are removed from results
		xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.outputPath());
		xml_found = parser.read(xmlPath);

		// set up xml page
		xmlPage = parser.page();
		xmlPage->setCreator(QString("CVL"));
		xmlPage->setImageSize(QSize(qImg.size()));
		xmlPage->setImageFileName(QFileInfo(mConfig.imagePath()).fileName());

		xmlPage->rootRegion()->removeAllChildren();
		for (auto tr : wsa.evalTextBlockRegions()) {
			xmlPage->rootRegion()->addChild(tr);
		}
		parser.write(xmlPath, xmlPage);
	}

	qInfo() << "White space layout analysis results computed in " << dt;
}

void WhiteSpaceTest::processDirectory(QString dirPath) {

	QDir dir(dirPath);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist!";
		return;
	}

	qInfo() << "Running White Space Analysis test on all *.tif and *.jpg files in directory: ";
	qInfo() << dirPath;

	//TODO use function for loading fileInfoList
	Timer dt;

	QStringList filters;
	filters << "*.tif" << "*.jpg";
	QFileInfoList fileInfoList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);

	int i = 0;
	for (auto f : fileInfoList) {
		++i;
		qDebug() << "processing image #" << QString::number(i) << " : " << f.absoluteFilePath();
		mConfig.setImagePath(f.absoluteFilePath());

		QFileInfo inputInfo(mConfig.imagePath());
		QString fileName = inputInfo.completeBaseName() + "." + inputInfo.completeSuffix();
		
		QString outputPath;
		if(QFileInfo(mConfig.outputPath()).isDir())
			outputPath = QFileInfo(mConfig.outputPath()).filePath() + "/" + fileName;
		else
			outputPath = QFileInfo(mConfig.outputPath()).path() + "/" + fileName;

		mConfig.setOutputPath(outputPath);
		run();
	}
	
	qInfo() << "Directory processed in " << dt;
}

void WhiteSpaceTest::testParameterSettings(QString dirPath){

	//define parameter settings
	//QString paramName = "textHeightMultiplier";
	//QVector<double> params = {1, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 2.75, 3};
	
	//QString paramName = "blackSeparators";
	//QVector<bool> params = {true, false};

	//QString paramName = "slicingSizeMultiplier";
	//QVector<double> params = {6, 7.5, 9};

	//QString paramName = "findWhiteSpaceGaps";
	//QVector<bool> params = {false, true};

	QString paramName = "polygonType";
	QVector<int> params = {0, 1, 2};
	
	QString initialPath = mConfig.outputPath();

	for (auto p : params) {

		QString outputPath;
		if (dirPath.isEmpty())
			outputPath = initialPath;
		else
			outputPath = dirPath;
		
		QFileInfo pathInfo(outputPath);

		if (pathInfo.isDir())
			outputPath = pathInfo.filePath();
		else
			outputPath = pathInfo.path();

		outputPath = outputPath + "/" + paramName + " = " + QString::number(p) + "/" + pathInfo.fileName();
		pathInfo.setFile(outputPath);
		outputPath = QFileInfo(outputPath).absoluteFilePath();
		QDir().mkpath(pathInfo.path());

		//change parameters in config
		mConfig.setOutputPath(outputPath);
		
		//mWsaConfig->setBlackSeparators(p);
		//mTlhConfig->setErrorMultiplier(p)
		//mWssConfig->setFindWhiteSpaceGaps(p);
		mTbfConfig->setPolygonType(p);

		//process image/directory
		if (dirPath.isEmpty())
			run();
		else
			processDirectory(dirPath);
	}
}

void WhiteSpaceTest::testFontHeightRatio(){

	QString sampleText = "this is a sample text created by David";

	//create document for rendering synthetic page image
	QTextDocument doc;
	QTextCursor cursor = QTextCursor(&doc);
	

	//apply random font styles
	QFont font;
	//font.fromString("Arial,-1,30,5,50,0,0,0,0,0");

	QTextCharFormat textFormat;
	//textFormat.setFont(font);
	cursor.insertText(sampleText, textFormat);

	QFontDatabase fdb;
	QStringList fontFamilies = fdb.families();
	
	double avgFontHeightRatio = 0;
	int numFonts = 0;

	for (auto fontName : fontFamilies) {
		font = QFont(fontName);
		font.setPixelSize(50);

		//change font
		cursor.movePosition(QTextCursor::Start);
		cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
		textFormat.setFont(font);
		cursor.setCharFormat(textFormat);

		QFontMetricsF fm(font);
		//double topMargin = cursor.block().blockFormat().topMargin();

		//int width = doc.size().toSize().width();
		//int leading = qRound(fm.leading());
		//double baselineY = doc.documentMargin() + leading + fm.ascent();
		//int baselineY = qRound(doc.documentMargin() + fm.ascent() - 1);
		//int xHeight = qRound(fm.xHeight());

		//QRectF bb = fm.boundingRect("x");
		//qDebug() << "xHeight = " << xHeight << ", bb.height" << bb.height();
		qDebug() << "fixedPitch() = " << font.fixedPitch();
		qDebug() << "exactMatch() = " << font.exactMatch();
		QRectF bb1 = fm.tightBoundingRect("acemnorsuvwxz");
		QRectF bb2 = fm.tightBoundingRect("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

		double xHeight_ = bb1.height();
		double lineHeight_ = bb2.height();
		double heightRatio = lineHeight_ / xHeight_;
		qDebug() << "xHeight_ = " << xHeight_ << ", lineHeight_ =" << lineHeight_ << ", heightRatio = " << heightRatio;
		//qDebug() << "xHeight = " << xHeight << "xHeight_ = " << xHeight_ << ", lineHeight_ =" << lineHeight_ << ", heightRatio = " << heightRatio;
		

		if (heightRatio != 1) {
			numFonts += 1;
			avgFontHeightRatio += heightRatio;
		}
		else
			qDebug() << "Skipped font (" << fontName << ") because heightRatio is equal to 1.";
			

		//QLine baseline(0, baselineY, width, baselineY);

		////draw synthetic text
		//doc.documentLayout();
		//QImage qImg = QImage(doc.size().toSize(), QImage::Format_ARGB32);
		//qImg.fill(QColor(255, 255, 255, 255)); //white background

		//QPainter p(&qImg);
		//doc.drawContents(&p);
		////p.setPen(ColorManager::blue());
		//p.drawLine(baseline);
		//
		//cv::Mat synthPageImg = Image::qImage2Mat(qImg);
		//qDebug() << "leading = " << leading << "; baselineY = " << baselineY << ", documentMargin = " << doc.documentMargin();
	}

	avgFontHeightRatio = avgFontHeightRatio / numFonts;
	qDebug() << "num font families = " << numFonts;
	qDebug() << "avgFontHeightRatio = " << avgFontHeightRatio;
}

//Font Style Classification Test ------------------------------------------------------------------------------
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

	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.imagePath());
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
	//fsc.compute();

	////draw results and compare with gt if available
	//cv::Mat img_result = fsc.draw(imgCv);
	//QString imgPath = Utils::createFilePath(xmlPath, "_fsc_results", "png");
	//Image::save(img_result, imgPath);

	//draw gabor filter bank - thesis image
	//GaborFilterBank gfb = fsc.createGaborFilterBank(QVector<double>(), QVector<double>());
	//QVector<cv::Mat> gfbImgs = gfb.draw();
	//cv::Mat spatialKernelImg = gfbImgs[0];
	//cv::Mat frequencyKernelImg = gfbImgs[1];
	//cv::Mat filterBankFR = gfbImgs[2];
}

void FontStyleClassificationTest::processDirectory(const QString dirPath){

	QDir dir(dirPath);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist!";
		return;
	}

	qInfo() << "Running font style classification test on all *.tif and *.jpg files in directory: ";
	qInfo() << dirPath;

	//TODO use function for loading fileInfoList
	Timer dt;

	QStringList filters;
	filters << "*.tif" << ".jpg";
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

void FontStyleClassificationTest::testSyntheticDataSet(QString fontDataDir, GaborFilterBank gfb, QVector<QFont> fonts, QString runIdentifier, int maxSampleCount, FontStyleClassifier::ClassifierMode cm, int k){

	//TODO store data sets (+ text patches) in a single file only
	//TODO make sure textureSize is adapted to training and test samples when loading them from file

	Timer dt;

	//param definitions
	int textureSize = 128;

	QString rtPath, trainDataSetPath, testDataSetPath, classifierPath, outputDir;
	QStringList samples_train, samples_test;
	FeatureCollectionManager fcm_train, fcm_test;

	//create paths
	fontDataDir = QFileInfo(fontDataDir).absoluteFilePath();

	QString pathAttribute;
	if (runIdentifier.isEmpty())
		pathAttribute = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
	else {
		runIdentifier += "; texSize=" + QString::number(textureSize);
		pathAttribute = runIdentifier;
	}
		

	outputDir = QFileInfo(fontDataDir + QDir::separator() + "FontStyleEvaluationResults_" + pathAttribute + QDir::separator()).absoluteFilePath();
	if (!QDir().mkpath(outputDir)) {
		qWarning() << "Unable to create font style classifier directory.";
		return;
	}
	
	rtPath = QFileInfo(fontDataDir, "FontTrainData_rt.txt").absoluteFilePath();
	trainDataSetPath = QFileInfo(outputDir, "FontStyleDataSet_train.txt").absoluteFilePath();
	testDataSetPath = QFileInfo(outputDir, "FontStyleDataSet_test.txt").absoluteFilePath();
	classifierPath = QFileInfo(outputDir, "FontStyleClassifier.txt").absoluteFilePath();

	if (maxSampleCount != -1) {
		QString classifierDir = QFileInfo(outputDir + QDir::separator() + QString::number(maxSampleCount) + QDir::separator()).absoluteFilePath();
		if (!QDir().mkpath(classifierDir)) {
			qWarning() << "Unable to create font style classifier directory.";
			return;
		}
		classifierPath = QFileInfo(classifierDir, "FontStyleClassifier.txt").absoluteFilePath();
	}

	outputDir = QFileInfo(classifierPath).absolutePath();
	
	//generate fonts used for test
	if (fonts.isEmpty()) {
		//TODO pass fontSize as additional input argument
		fonts = FontDataGenerator::generateFonts(4, { "Arial" });
		int fontSize = 30; 
		fonts[0].setPixelSize(fontSize);
		fonts[1].setPixelSize(fontSize);
		fonts[2].setPixelSize(fontSize);
		fonts[3].setPixelSize(fontSize);
	}

	if(gfb.isEmpty())
		gfb = generateGFB();

	int patchHeightEstimate = -1;

	//load or generate data sets from file
	if (!QFileInfo(trainDataSetPath).exists() || !QFileInfo(testDataSetPath).exists()) {

		QStringList wordList = loadWordSamples(rtPath, 0, false);
		if (wordList.isEmpty())
			return;
		
		QVector<QStringList> sampleSets;
		
		auto trainWords = wordList.mid(0, 2500);
		//trainWords.removeDuplicates();
		trainWords = trainWords.mid(0, 100);
		
		auto testWords = wordList.mid(2500);
		//testWords.removeDuplicates();
		testWords = testWords.mid(0, 750);

		//filter words according to length
		//int minWordLength = 4;
		//int maxWordLength = 100;
		//QStringList filteredWords;
		//for (auto word : testWords) {
		//	if (word.length() < minWordLength || word.length() > maxWordLength)
		//		continue;			
		//	filteredWords.append(word);
		//}

		sampleSets << trainWords;
		sampleSets << testWords;

		//compute patch height estimate
		patchHeightEstimate = FontDataGenerator::computePatchSizeEstimate(sampleSets[0], fonts);
		
		////debug images
		//LabelManager labelManager = FontDataGenerator::generateFontLabelManager(fonts);
		//auto textPatches_train = FontDataGenerator::generateTextPatches(sampleSets[0], labelManager, patchHeightEstimate, gfb.kernelSize());
		//cv::Mat trainPatchesImg = FontDataGenerator::drawTextPatches(textPatches_train);
		//cv::Mat trainPatchesTextureImg = FontDataGenerator::drawTextPatches(textPatches_train, true);
		//auto textPatches_test = FontDataGenerator::generateTextPatches(sampleSets[1], labelManager, patchHeightEstimate, gfb.kernelSize());
		//cv::Mat testPatchesImg = FontDataGenerator::drawTextPatches(textPatches_test);
		//cv::Mat testPatchesTextureImg = FontDataGenerator::drawTextPatches(textPatches_test, true);

		//TODO write patchHeight to data set files
		if (!FontDataGenerator::generateDataSet(sampleSets[0], fonts, gfb, trainDataSetPath, patchHeightEstimate, textureSize) ||
			!FontDataGenerator::generateDataSet(sampleSets[1], fonts, gfb, testDataSetPath, patchHeightEstimate, textureSize))
			return;
	}

	//TODO read patchHeight from data set files
	//TODO read gfb from file and match with generated gfb (or replace it)
	if (!FontDataGenerator::readDataSet(trainDataSetPath, fcm_train, samples_train) || 
		!FontDataGenerator::readDataSet(testDataSetPath, fcm_test, samples_test))
		return;

	//reduce training data set size (for evaluation purpose)
	if (maxSampleCount > 0 && samples_train.size() > maxSampleCount) {
		reduceSampleCount(fcm_train, maxSampleCount);
		samples_train = samples_train.mid(0, maxSampleCount);
		qDebug() << "Number of training samples reduced to: " << samples_train.size();
	}

	qInfo() << "Number of train/test samples: " << samples_train.size() << "/" << samples_test.size();
	
	//load or generate font style classifier
	QSharedPointer<FontStyleClassifier> fsClassifier;
	if (QFileInfo(classifierPath).exists()) {
		qInfo() << "Loading classifier from existing file: " << classifierPath;

		fsClassifier = FontStyleClassifier::read(classifierPath);
		if (fsClassifier->isEmpty()) {
			qInfo()		<< "Delete existing file to generate a new classifier.";
			return;
		}
	}
	else{
		//TODO replace section with trainFontStyleClassifier function
		//TODO get texture size and gabor params from train data 
		auto fst = FontStyleTrainer(FontStyleDataSet(fcm_train, patchHeightEstimate, gfb)); // TODO read text height estimate from data set file; consider removing input value

		//test different classfier modes
		auto fstConfig = fst.config();
		fstConfig->setModelType(cm);
		fstConfig->setDefaultK(k);

		if (!fst.compute()) {
			qCritical() << "Failed to train font style classifier.";
			return;
		}

		fsClassifier = fst.classifier();
		fsClassifier->write(classifierPath);	//optional
	}

	//generate test data
	auto labelManager = fcm_train.toLabelManager();
	auto textPatches_test = FontDataGenerator::generateTextPatches(samples_test, labelManager, patchHeightEstimate, textureSize);

	//compute font style classification results
	//TODO consider removing text patches as input (just return false if data set path is empty/false/invalid)
	FontStyleClassification fsc = FontStyleClassification(textPatches_test, testDataSetPath);
	fsc.setClassifier(fsClassifier);

	if (!fsc.compute()) {
		qCritical() << "Failed to compute font style classification results";
		return;
	}

	//compute evaluation results
	auto textPatches = fsc.textPatches();
	double op = evalTextPatchResults(textPatches, labelManager, outputDir);

	QString resultsFilePath = QFileInfo(fontDataDir, "GatheredEvalResults.txt").absoluteFilePath();
	appendOverallPerformance(op, resultsFilePath, runIdentifier, outputDir);

	qInfo() << "Finished font style classification test on synthetic data in "  << dt << ".";
}

void FontStyleClassificationTest::testSyntheticPage(QString pageDataPath, QString trainDataPath) {

	//TODO add possibility to train classifier from text lines instead of patches
	//TODO write/read multiple label tags in custom string of regions
	//TODO use additional style label parameter for differentiation of gt/predicted labels

	int maxSamples = 2000;	//max num of sample used for training classifier for synthetic page

	//generate fonts used in synthetic page
	//QVector<QFont> pageFonts = generateFonts(4, { "Arial" });
	
	QVector<QFont> pageFonts;
	QFont font;
	font.fromString("Arial,-1,30,5,50,0,0,0,0,0");
	pageFonts << font;
	font.fromString("Franklin Gothic Medium,-1,30,5,75,0,0,0,0,0");
	pageFonts << font;
	font.fromString("Times New Roman,-1,30,5,75,1,0,0,0,0");
	pageFonts << font;
	font.fromString("Georgia,-1,30,5,50,1,0,0,0,0");
	pageFonts << font;

	//generate gabor filter bank
	GaborFilterBank gfb = generateGFB();

	//generate synthetic page image (+ GT)
	cv::Mat synthPageCv;

	QString syntheticImagePath = QFileInfo(pageDataPath).absolutePath();
	syntheticImagePath = QFileInfo(syntheticImagePath, "syntheticPage.png").absoluteFilePath();
	QString outputDir = QFileInfo(pageDataPath).absolutePath();

	//load or generate synthetic page image
	if (!QFileInfo(syntheticImagePath).exists()) {
		if (!generateSnytheticTestPage(pageDataPath, syntheticImagePath, pageFonts)) {
			qDebug() << "Failed to generate synthetic page image.";
			return;
		}
	}
	
	synthPageCv = Image::qImage2Mat(QImage(syntheticImagePath));

	if (synthPageCv.empty()) {
		qCritical() << "Failed to load synthetic page image.";
		return;
	}

	//generate training data set for classifier (if files do not exist)
	QString trainDataSetPath = Utils::createFilePath(trainDataPath, "_fcm_train", "txt");
	if (!QFileInfo(trainDataSetPath).exists()) {
		QStringList wordList = loadWordSamples(trainDataPath);
		wordList = wordList.mid(0, maxSamples);	//reduce amount of training samples
		if (wordList.isEmpty())
			return;

		FontDataGenerator::generateDataSet(wordList, pageFonts, gfb, trainDataSetPath);
	}

	//load data sets
	QStringList samples_train;
	FeatureCollectionManager fcm_train;

	if (!FontDataGenerator::readDataSet(trainDataSetPath, fcm_train, samples_train))
		return;

	//load or generate font style classifier
	QString classifierDir = QFileInfo(trainDataSetPath).absolutePath();
	QString classifierPath = QFileInfo(classifierDir, "FontStyleClassifier.txt").absoluteFilePath();

	//load or generate font style classifier
	QSharedPointer<FontStyleClassifier> fsClassifier;
	if (QFileInfo(classifierPath).exists()) {
		qInfo() << "Loading existing classifier from  file: " << classifierPath;

		fsClassifier = FontStyleClassifier::read(classifierPath);
		if (fsClassifier->isEmpty())
			return;
	}
	else {
		qInfo() << "Generating new font style classifier from training data.";
		auto fst = FontStyleTrainer(FontStyleDataSet(fcm_train, -1, gfb));

		if (!fst.compute())
			return;

		fsClassifier = fst.classifier();
		fsClassifier->write(classifierPath);	//optional
	}

	//get label manager
	LabelManager lm = fsClassifier->manager();
	
	//compute font style classification results for text lines
	QVector<QSharedPointer<TextLine>> textLineRegions = loadTextLines(syntheticImagePath);

	FontStyleClassification fsc = FontStyleClassification(synthPageCv, textLineRegions);
	fsc.setClassifier(fsClassifier);

	if (!fsc.compute()) {
		qCritical() << "Failed to compute font style classification results";
		return;
	}

	auto resultPatches = fsc.textPatches();

	//map style results to GT regions and compute evaluation results
	auto gtRegions = FontDataGenerator::loadRegions<TextRegion>(syntheticImagePath, Region::type_text_region);

	////remove patches of short words
	//QVector<QSharedPointer<TextRegion>> longWordRegions;
	//for (auto wr : gtRegions) {
	//	if (wr->text().length() >= 4)
	//		longWordRegions << wr;
	//}
	//gtRegions = longWordRegions;

	//TODO replace this function with generateTextPatches() -> incorporate patchHeight parameter and patchHeightEstimation()
	//TODO unify and simplify the functions generateTextPatches() and regionsToTextPatches()
	auto gtPatches = regionsToTextPatches(gtRegions, lm, synthPageCv);
	fsc.mapStyleToPatches(gtPatches);

	//compute evaluation results based on GT regions
	evalTextPatchResults(gtPatches, lm, outputDir);

	//debug/visualize results
	//cv::Mat predResults_gtPtaches = fsc.draw(synthPageCv, gtPatches, FontStyleClassification::draw_patch_results);
	//cv::Mat compResults_gtPtaches = fsc.draw(synthPageCv, gtPatches, FontStyleClassification::draw_comparison);
	//cv::Mat gtPtaches = fsc.draw(synthPageCv, gtPatches, FontStyleClassification::draw_gt);
	//cv::Mat predResults = fsc.draw(synthPageCv);
}

void FontStyleClassificationTest::testCatalogueRegions(QString dirPath, GaborFilterBank gfb, QString runIdentifier, FontStyleClassifier::ClassifierMode cm, int k){

	//param definitions
	int textureSize = 128;
	bool drawDebugImages = false;

	//create directories
	QString pathAttribute;
	if (runIdentifier.isEmpty())
		pathAttribute = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
	else {
		runIdentifier += "; texSize=" + QString::number(textureSize);
		pathAttribute = runIdentifier;
	}

	QString outputDir = QFileInfo(dirPath + QDir::separator() + "FontStyleEvaluationResults " + pathAttribute + QDir::separator()).absoluteFilePath();
	if (!QDir().mkpath(outputDir)) {
		qWarning() << "Unable to create font style output directory.";
		return;
	}
	
	QString trainDir = QFileInfo(dirPath, "train").absoluteFilePath();
	QString testDir = QFileInfo(dirPath, "test").absoluteFilePath();

	//find estimate for patchSize parameter for adaptive patch generation
	//TODO save patch size estimate together with classifier and load it from file
	int pse = FontDataGenerator::computePatchSizeEstimate(trainDir);

	if (pse == -1) {
		qCritical() << "Patch size estimation failed! Please specify training directory with text regions (xml)!";
		return;
	}
	
	qInfo() << "patchSizeEstimate = " << pse;
	
	//generate gabor filter bank
	if(gfb.isEmpty())
		gfb = generateGFB();

	//load or train font style classifier
	QString classifierFilePath = QFileInfo(outputDir, "FontStyleClassifier.txt").absoluteFilePath();
	QSharedPointer<FontStyleClassifier> fsClassifier;
	if (!QFileInfo(classifierFilePath).exists()) {
		fsClassifier = trainFontStyleClassifier(trainDir, gfb, pse, textureSize, cm, k, outputDir);
	}	
	else
		fsClassifier = FontStyleClassifier::read(classifierFilePath);

	if (!fsClassifier->isTrained()) {
		qCritical() << "Failed to load/train font style classififer.";
		return;
	}

	//get LabelManager for generation of test data (ensure that labels (IDs) match training data)
	LabelManager lm = fsClassifier->manager();

	//TODO test extraction of patches from text line regions (or word regions)

	//process GT regions of input images
	QFileInfoList fileInfoList = Utils::getImageList(testDir);
	QVector<QSharedPointer<TextPatch>> textPatches_result = QVector<QSharedPointer<TextPatch>>();

	for (auto f : fileInfoList) {
		
		QString imagePath = f.absoluteFilePath();
		auto imagePatches = FontDataGenerator::generateTextPatches(imagePath, pse, QSharedPointer<LabelManager>::create(lm), textureSize);

		if (imagePatches.isEmpty()) {
			qWarning() << "No text patches generated! Skipping image.";
			continue;
		}

		//compute font style classification results
		FontStyleClassification fsc = FontStyleClassification(imagePatches);
		fsc.setClassifier(fsClassifier);

		if (!fsc.compute()) {
			qCritical() << "Failed to compute font style classification results";
			return;
		}

		//get evaluation results
		auto imagePatchesResults = fsc.textPatches();
		textPatches_result << imagePatchesResults;
		bool xmlOk = patchesToXML(imagePatchesResults, imagePath);

		if (!xmlOk)
			qWarning() << "Could not extract FSC results to output xml!";

		//per page results
		double pagePerformance = evalTextPatchResults(imagePatches, lm, outputDir, imagePath);

		//draw debug image showing results
		if (drawDebugImages) {
			QImage qImg(imagePath);
			cv::Mat imgCv = Image::qImage2Mat(qImg);
			cv::Mat predResults_gtPtaches = fsc.draw(imgCv, imagePatchesResults, FontStyleClassification::draw_patch_results);
			cv::Mat compResults_gtPtaches = fsc.draw(imgCv, imagePatchesResults, FontStyleClassification::draw_comparison);
			cv::Mat trueResults_gtPtaches = fsc.draw(imgCv, imagePatchesResults, FontStyleClassification::draw_gt);
			
			QString att = "_result_";			
			QString imgPath = Utils::createFilePath(imagePath, att, "png");
			QImage qDebugImg = Image::mat2QImage(predResults_gtPtaches);
			qDebugImg.save(imgPath, 0, 1);
			
			att = "_comp_";
			imgPath = Utils::createFilePath(imagePath, att, "png");
			qDebugImg = Image::mat2QImage(compResults_gtPtaches);
			qDebugImg.save(imgPath, 0, 1);
			
			att = "_gt_";
			imgPath = Utils::createFilePath(imagePath, att, "png");
			qDebugImg = Image::mat2QImage(trueResults_gtPtaches);
			qDebugImg.save(imgPath, 0, 1);
		}
	}
	
	double overallPerformance = evalTextPatchResults(textPatches_result, lm, outputDir, testDir);
	
	//gather eval results of multiple runs
	QString resultsFilePath = QFileInfo(dirPath, "GatheredEvalResults.txt").absoluteFilePath();
	appendOverallPerformance(overallPerformance, resultsFilePath, runIdentifier);

	resultsFilePath = QFileInfo(dirPath).dir().absolutePath();
	resultsFilePath = QFileInfo(resultsFilePath, "GatheredEvalResults.txt").absoluteFilePath(); 	//output to parent directory too
	appendOverallPerformance(overallPerformance, resultsFilePath, runIdentifier);
}

bool FontStyleClassificationTest::patchesToXML(QVector<QSharedPointer<TextPatch>> textPatches, QString imagePath){

	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(imagePath);
	xmlPath = Utils::createFilePath(xmlPath, "_patchResults");
	QImage qImg(imagePath);

	if (qImg.isNull()) {
		qWarning() << "Could NOT load image for file: " << imagePath;
		return false;
	}

	QVector<QSharedPointer<TextLine>> tprs;
	for (auto tp : textPatches) {
		QSharedPointer<TextLine> tl = QSharedPointer<TextLine>::create(Region::Type::type_word);
		tl->setPolygon(tp->polygon());
		tl->setCustom(tp->label()->predicted().name());
		tprs << tl;
	}

	rdf::PageXmlParser parser;
	QSharedPointer<PageElement> xmlPage;
	parser.read(xmlPath);

	// set up xml page
	xmlPage = parser.page();
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(QSize(qImg.size()));
	xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	//add results to xml
	xmlPage = parser.page();
	xmlPage->rootRegion()->removeAllChildren();

	for (auto tl : tprs) {
		xmlPage->rootRegion()->addChild(tl);
	}

	parser.write(xmlPath, xmlPage);

	return true;
}

void FontStyleClassificationTest::runTestSuite(QString synthDataSetPath, QString catalogueDataSetPath) {

	bool doCatTests = true;
	bool doSynthTests = true;
	bool testClassifiers = false;

	QVector<QVector<QFont>> fontSets = generateTestFontStyleSets();
	QVector<GaborFilterBank> gfbs = generateTestGaborFilterBanks();

	QVector<QString> catalogueSets;
	catalogueSets << "F:/dev/da/data/catalogue/fsc_selection/1907_Brussels_EGBA";
	catalogueSets << "F:/dev/da/data/catalogue/fsc_selection/1907_Paris_SdA";
	catalogueSets << "F:/dev/da/data/catalogue/fsc_selection/1905_Venice_EI";

	//run tests on synthetic data sets
	if (doSynthTests) {
		for (auto gfb : gfbs) {
			int fontIdx = 0;
			for (auto fontSet : fontSets) {
				QString runIdentifier;

				runIdentifier += " fontSetIdx=" + QString::number(fontIdx) + "; ";
				runIdentifier += "kSize=" + QString::number(gfb.kernelSize()) + "; ";
				runIdentifier += "l=";
				for (auto l : gfb.lambda())
					runIdentifier += QString::number(l, 'f', 1) + ", ";

				runIdentifier.chop(2);
				runIdentifier += +"; ";
				runIdentifier += " t=";
				for (auto t : gfb.theta())
					runIdentifier += QString::number(t / DK_DEG2RAD) + ", ";
				runIdentifier.chop(2);

				double sigmaMultiplier = gfb.sigmaMultiplier();
				if (sigmaMultiplier != -1) {
					runIdentifier += +"; ";
					runIdentifier += " s=" + QString::number(sigmaMultiplier, 'f', 1);
				}

				runIdentifier.trimmed();

				//test classifier types
				if (testClassifiers) {
					for (int i = 0; i < FontStyleClassifier::classify_end; ++i) {
						QString runIdentifier_ = runIdentifier + "; cm=" + QString::number(i);
						runIdentifier_.trimmed();
						testSyntheticDataSet(synthDataSetPath, gfb, fontSet, runIdentifier_, (FontStyleClassifier::ClassifierMode) i);
					}
				}
				else {
					testSyntheticDataSet(synthDataSetPath, gfb, fontSet, runIdentifier);
				}

				fontIdx += 1;
			}
		}
	}

	//run tests on catalogue data sets
	if (doCatTests) {
		for (auto gfb : gfbs) {
			int catIdx = 0;
			for (auto catSet : catalogueSets) {
				QString runIdentifier;

				runIdentifier += "catSetIdx=" + QString::number(catIdx) + "; ";
				runIdentifier += "kSize=" + QString::number(gfb.kernelSize()) + "; ";

				runIdentifier += " l=";
				for (auto l : gfb.lambda())
					runIdentifier += QString::number(l, 'f', 1) + ", ";

				runIdentifier.chop(2);
				runIdentifier += +"; ";
				runIdentifier += " t=";
				for (auto t : gfb.theta())
					runIdentifier += QString::number(t / DK_DEG2RAD) + ", ";
				runIdentifier.chop(2);

				double sigmaMultiplier = gfb.sigmaMultiplier();
				if (sigmaMultiplier != -1) {
					runIdentifier += +"; ";
					runIdentifier += " s=" + QString::number(sigmaMultiplier, 'f', 1);
				}

				runIdentifier.trimmed();

				//test classifier types
				if (testClassifiers) {
					for (int i = 0; i < FontStyleClassifier::classify_end; ++i) {
						QString runIdentifier_ = runIdentifier + "; cm=" + QString::number(i);
						runIdentifier_.trimmed();
						testCatalogueRegions(catSet, gfb, runIdentifier_, (FontStyleClassifier::ClassifierMode) i);
					}
				}
				else {
					testCatalogueRegions(catSet, gfb, runIdentifier);
				}

				catIdx += 1;
			}
		}
	}
}

void FontStyleClassificationTest::drawDebugImages() {

	//images used for debugging and documentation purpose

	bool drawSynSet = false;
	bool drawGaborFilterBank = false;
	bool drawTextPatches = false;

	if (drawGaborFilterBank) {
		//draw gabor filter bank
		QVector<double> theta = { 0, 30, 60, 90 };
		QVector<double> lambda = { 2, 4, 8, 16 };
		GaborFilterBank gfb = GaborFilterBank(lambda, theta, 1024, 1.4);
		QVector<cv::Mat> gfbImgs = gfb.draw();
		cv::Mat spatialKernelImg = gfbImgs[0];
		cv::Mat frequencyKernelImg = gfbImgs[1];
		cv::Mat filterBankFR = gfbImgs[2];

		cv::Mat normalizedImg, normalizedImg2;
		cv::normalize(filterBankFR, normalizedImg, 0, 1, cv::NORM_MINMAX);

		cv::Mat filterBankFR2 = filterBankFR;
		cv::normalize(filterBankFR, filterBankFR2, 0, 255, cv::NORM_MINMAX);
		filterBankFR2 = filterBankFR.mul(filterBankFR);
		filterBankFR2 = filterBankFR2.mul(filterBankFR2);
		cv::normalize(filterBankFR2, normalizedImg2, 0, 1, cv::NORM_MINMAX);

		//draw labeled text patches from image + xml file
		//QString imagePath = "F:/dev/da/data/catalogue/fsc_selection/1905_Venice_EI/train/1905_Venice_Biennale_0041.jpg";
		//QString imagePath = "F:/dev/da/data/catalogue/fsc_selection/1907_Brussels_EGBA/train/1907_Brussels_BeauxArts_0015.jpg";
		QString imagePath = "F:/dev/da/data/catalogue/fsc_selection/1907_Paris_SdA/train/1907_Paris_SalondAutomne_0040.jpg";

		auto imagePatches = FontDataGenerator::generateTextPatches(imagePath);

		QImage qImg(imagePath);
		cv::Mat imgCv = Image::qImage2Mat(qImg);

		FontStyleClassification fsc = FontStyleClassification(imagePatches);
		cv::Mat trueResults_gtPtaches = fsc.draw(imgCv, imagePatches, FontStyleClassification::draw_gt);
	}

	///////////////////////////////////////////////////////////////////////////
	//draw synthetic text images visualizing synthetic data sets Syn1-3
	if(drawSynSet){
		//create document for rendering synthetic page image
		QTextDocument doc;
		QTextCursor cursor = QTextCursor(&doc);
		//QString sampleText = "\n\n     \tsample  text\t\t sample  text     \t\n\n\     \tsample  text\t\t sample  text\n\n";
		QString sampleText = "\n\n     \tsample  text \t\tsample  text \tsample  text\t\n\n\ \tsample  text \tsample  text\t\n\n";
		cursor.insertText(sampleText);

		//generate font style
		auto fontSets = generateTestFontStyleSets();
		QVector<QFont> fontSet = fontSets[0];

		QTextCharFormat textFormat;
		textFormat.setFont(fontSet[0]);

		LabelManager lm = FontDataGenerator::generateFontLabelManager(fontSet);

		int id = 1;
		for (auto l : lm.labelInfos()) {
			QColor color = ColorManager::getColor(l.id(), 1.0);
			qDebug() << "Label " << l.toString() << "; Color: " << color.red() << color.green() << color.blue();
			++id;
		}

		int wCount = 0;
		int fontIdx = 0;
		cursor.movePosition(QTextCursor::Start);
		while (!cursor.atEnd()) {
			cursor.select(QTextCursor::WordUnderCursor);
			if (!cursor.selectedText().isEmpty()) {

				if (fontIdx >= fontSet.size())
					fontIdx = 0;

				textFormat.setFont(fontSet[fontIdx]);
				cursor.setCharFormat(textFormat);
				++wCount;

				if (wCount % 2 == 0)
					++fontIdx;
			}
			cursor.movePosition(QTextCursor::NextWord);
		}

		//draw synthetic text page
		QImage qImg = QImage(doc.size().toSize(), QImage::Format_ARGB32);
		qImg.fill(QColor(255, 255, 255, 255)); //white background

		QPainter p(&qImg);
		doc.documentLayout();
		doc.drawContents(&p);
		cv::Mat synthPageImg = Image::qImage2Mat(qImg);

		QString outputFilePath = "F:/dev/da/data/SynthFontData/syn2.png";
		if (Image::save(synthPageImg, outputFilePath)) {
			if (!generateGroundTruthData(doc, outputFilePath))
				qWarning() << "Failed to generate ground truth file for synthetic page.";
			qInfo() << "Synthetic page image saved to: " << outputFilePath;
		}
		else
			qDebug() << "Failed to save synth doc image.";

		QVector<QSharedPointer<TextRegion>> wordRegions = FontDataGenerator::loadRegions<TextRegion>(outputFilePath, Region::type_text_region);
		QVector<QSharedPointer<TextPatch>> tps = regionsToTextPatches(wordRegions, lm, synthPageImg);

		FontStyleClassification fsc = FontStyleClassification(tps);
		cv::Mat syntheticGt = fsc.draw(synthPageImg, tps, FontStyleClassification::draw_gt);
	}

	///////////////////////////////////////////////////////////////////////////
	//draw synthetic text patches visualizing text patch generation
	if (drawTextPatches) {

		QFont f;
		f.fromString("Arial,-1,120,5,50,0,0,0,0,0");
		LabelInfo li = LabelInfo(1, FontStyleClassification::fontToLabelName(f));

		TextPatch tps = TextPatch("A texture generation sample.", li, -1, 128, true);
		cv::Mat sentenceImg = tps.textPatchImg();

		TextPatch tp = TextPatch("sample", li, 108, 512, true);
		cv::Mat patchImg = tp.textPatchImg();
		cv::Mat texImg = tp.patchTexture();

		TextPatch tp0 = TextPatch("sample", li, 78, 128, true);
		cv::Mat patchImg0 = tp0.textPatchImg();

		TextPatch tp1 = TextPatch("sample", li, 138, 128, true);
		cv::Mat patchImg1 = tp1.textPatchImg();
	}
}

QVector<GaborFilterBank> FontStyleClassificationTest::generateTestGaborFilterBanks() {
	QVector<GaborFilterBank> gfbs;
	
	int kernelSize = 128;
	double sigma = 1.4;
	QVector<double> theta = { 0, 30, 60, 90 };
	QVector<double> lambda = { 2, 4, 8, 16 };

	//for (int i = 0; i < lambda.size(); ++i)
	//	lambda[i] = lambda[i] * sqrt(2);

	GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	gfbs << gfb;

	///////////////////////////////////////////
	//sigma multiplier variation
	//for (double sig = 4.0; sig <= 4.0; sig+=0.2) {
	//	GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sig);
	//	gfbs << gfb;
	//	//qDebug() << gfb.toString();
	//}
	
	///////////////////////////////////////////
	//theta variation
	//int stepSize = 15;
	//for (double t = 0; t < 180; t+=+stepSize) {
	//	
	//	QVector<double> theta = {};
	//	//QVector<double> theta = { 0, 30, 60, 90 };
	//	
	//	if (theta.contains(t))
	//		continue;
	//	else
	//		theta << t;
	//	//GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize);
	//	GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, 1.4);
	//	gfbs << gfb;
	//	//qDebug() << gfb.toString();
	//}
	//selected theta sets
	//sigma = 1.4;
	//lambda = { 2, 4, 8, 16 };
	//theta = { 0, 60, 90 };
	//GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 30, 60, 90 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 15, 60, 90 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 60, 90, 135 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 45, 90, 135 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 30, 45, 90 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 30, 60, 90, 135};
	//lambda = { 2, 4, 8, 16 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;	
	//theta = { 0, 30, 60, 90, 120};
	//lambda = { 2, 4, 8, 16 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 30, 60, 90, 120, 150 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//theta = { 0, 22, 45, 67, 90, 112, 135, 157 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;

	///////////////////////////////////////////
	//lambda variation
	//sigma = 1.4;
	//theta = { 0, 45, 90, 135 };
	//lambda = { 2, 4, 8, 16 };
	//single lambda values
	//for (double l = 2; l * 4 <= kernelSize; l = l * 2) {
	//	GaborFilterBank gfb = GaborFilterBank(QVector<double>() << l, theta, kernelSize, sigma);
	//	gfbs << gfb;
	//	//qDebug() << "lambda = " << l;
	//}
	////selected lambda sets
	//lambda = { 2, 4, 8 };
	//GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//lambda = { 2, 4, 8, 16 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//lambda = { 2, 3, 4, 6, 8 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//lambda = { 2, 4, 8, 16, 32 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//lambda = { 2, 3, 4, 6, 8, 12, 16 };
	//gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//gfbs << gfb;
	//auto gbfs2 = gfbs;
	//sets with additional multiplier
	//for (int i = 0; i < gbfs2.size(); ++i) {
	//	lambda = gbfs2[i].lambda();
	//	
	//	for (int i = 0; i < lambda.size(); ++i)
	//		lambda[i] = lambda[i] * sqrt(2);
	//	gfb = GaborFilterBank(lambda, theta, kernelSize, sigma);
	//	gfbs << gfb;
	//}

	return gfbs;
}

QVector<QVector<QFont>> FontStyleClassificationTest::generateTestFontStyleSets(){

	QFont fontStyle;
	QVector<QFont> fontStyleSet;
	QVector<QVector<QFont>> fontStyleManager;

	//---------------------------------------------------------------------------------------------------------------------
	//4 different styles
	//fontStyle.fromString("Arial,30,-1,5,50,0,0,0,0,0");	//using pointSize instead of pixelSize
	fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0"); //regular
	fontStyleSet << fontStyle;

	fontStyle.fromString("Arial,-1,30,5,75,0,0,0,0,0"); //bold
	fontStyleSet << fontStyle;

	fontStyle.fromString("Arial,-1,30,5,75,1,0,0,0,0"); //bold + italic
	fontStyleSet << fontStyle;

	fontStyle.fromString("Arial,-1,30,5,50,1,0,0,0,0"); //italic
	fontStyleSet << fontStyle;

	fontStyleManager << fontStyleSet;

	//---------------------------------------------------------------------------------------------------------------------
	//4 different font families
	fontStyleSet.clear();
	fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0");
	QVector<QString> fontFamilies = { "Arial", "Franklin Gothic Medium", "Times New Roman", "Georgia" };
	for (auto fontFamily : fontFamilies) {
		QFont f = fontStyle;
		f.setFamily(fontFamily);
		fontStyleSet << f;
	}
	fontStyleManager << fontStyleSet;

	//---------------------------------------------------------------------------------------------------------------------
	//5 different font sizes 1 style
	fontStyleSet.clear();
	fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0");
	int fontSize = 10;
	int fontStepSize = 10;

	for (int i = 0; i < 5; ++i) {
		QFont f = QFont(fontStyle);
		f.setPixelSize(10+10*i);
		fontStyleSet << f;
		//qDebug() << f.toString();
	}
	fontStyleManager << fontStyleSet;

	//---------------------------------------------------------------------------------------------------------------------
	//varying font size for 4 different fonts (varying in typeface or style)
	//4 typeface
	//fontStyleSet.clear();
	//fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0");
	//QVector<QString> fontFamilies = { "Arial", "Franklin Gothic Medium", "Times New Roman", "Georgia" };
	//for (auto fontFamily : fontFamilies) {
	//	QFont f(fontStyle);
	//	f.setFamily(fontFamily);
	//	fontStyleSet << f;
	//}
	////4 styles
	//fontStyleSet.clear();
	//fontStyle.fromString("Arial,-1,30,5,50,0,0,0,0,0"); //regular
	//fontStyleSet << fontStyle;
	//fontStyle.fromString("Arial,-1,30,5,75,0,0,0,0,0"); //bold
	//fontStyleSet << fontStyle;
	//fontStyle.fromString("Arial,-1,30,5,75,1,0,0,0,0"); //bold + italic
	//fontStyleSet << fontStyle;
	//fontStyle.fromString("Arial,-1,30,5,50,1,0,0,0,0"); //italic
	//fontStyleSet << fontStyle;
	////fontStyleManager << fontStyleSet;
	//int fontSize = 10;
	//int fontStepSize = 10;
	//for (int i = 0; i < 10; ++i) {
	//	QVector<QFont> newSet;
	//	for (auto f : fontStyleSet) {
	//		f.setPixelSize(10 + 10 * i);
	//		newSet << f;
	//	}
	//	fontStyleManager << newSet;
	//}
	//debugging
	////for (auto s : fontStyleManager) {
	////	qDebug() << "";
	////	for (auto f : s)
	////		qDebug() << f.toString();
	////}

	return fontStyleManager;
}

void FontStyleClassificationTest::reduceSampleCount(FeatureCollectionManager & fcm, int sampleCount) const{

	FeatureCollectionManager fcm_;

	if (sampleCount < fcm.collection()[0].numDescriptors()) {
		for (FeatureCollection & c : fcm.collection()) {
			if (c.numDescriptors() > sampleCount) {
				cv::Mat desc_temp = c.descriptors().rowRange(cv::Range(0, sampleCount));
				c.setDescriptors(desc_temp);
			}
			fcm_.add(c);
		}
		fcm = fcm_;                                                                                                                                                                            
	}
}

GaborFilterBank FontStyleClassificationTest::generateGFB(){
	
	//define gabor filter bank
	QVector<double> theta = { 0, 30, 60, 90 };
	QVector<double> lambda = { 2, 4, 8, 16 };

	double sigmaMultiplier = 1.4;
	int kernelSize = 128;

	//for (int i = 0; i < lambda.size(); i++)
	//	lambda[i] *= sqrt(2);

	GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sigmaMultiplier);

	//debug outputs
	//qDebug().noquote() << gfb.toString();
	//auto gfb_imgs = gfb.draw();
	//cv::Mat gfb_img0 = gfb_imgs[0];
	//cv::Mat gfb_img1 = gfb_imgs[1];
	
	return gfb;
}

QStringList FontStyleClassificationTest::loadWordSamples(QString filePath, int minWordLength, bool removeDuplicates) {

	QString inputText = readTextFromFile(filePath);

	if (inputText.isEmpty())
		return QStringList();

	//use alphanumeric symbols only 
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[-`~!@#$%^&*()_â€”+=|:;<>Â«Â»,.?/{}\'\"\\\[\\\]\\\\]")), " ");

	//remove formatting symbols
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[\\n\\t\\r]")), " ");

	QStringList wordListTmp = inputText.split(' ');

	if (removeDuplicates)
		wordListTmp.removeDuplicates();

	//remove short words
	QStringList wordList;
	for (auto word : wordListTmp) {
		if (word.isEmpty())
			continue;
		if (word.length() >= minWordLength)
			wordList.append(word);
	}

	if (wordList.size() < 50) {
		qCritical() << "No data set created. Please provide a text file containing (>50) unique words.";
		qCritical() << "Each word should have a minimum of " << minWordLength << " characters.";
		return QStringList();
	}

	double avgLength = 0;
	for (auto word : wordList) {
		avgLength += word.length();
	}
	avgLength = avgLength / wordList.size();
	qDebug() << "average word length in sample set = " << QString::number(avgLength);

	return wordList;
}

QString FontStyleClassificationTest::readTextFromFile(QString filePath) {

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open file: " << filePath;
		return QString();
	}

	QTextStream inStream(&file);
	QString inputText = inStream.readAll();
	inStream.flush();
	file.close();

	if (inputText.isEmpty()) {
		qInfo() << "Input file is empty: " << filePath;
		return QString();
	}

	return inputText;
}

QVector<QStringList> FontStyleClassificationTest::splitSampleSet(QStringList sampleSet, double ratio) {

	if (sampleSet.isEmpty()) {
		qWarning() << "Sample set is empty.";
		return QVector<QStringList>();
	}

	int wNum = sampleSet.size();
	int s = (int)std::floor((double)wNum*ratio);

	QStringList wordListTrain = sampleSet.mid(0, s);
	QStringList wordListTest = sampleSet.mid(s);

	QVector<QStringList> splitSets = {wordListTrain, wordListTest};

	return splitSets;
}

QVector<QSharedPointer<TextLine>> FontStyleClassificationTest::loadTextLines(QString imagePath){
	
	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(imagePath);

	rdf::PageXmlParser parser;
	bool xml_found = parser.read(xmlPath);

	if (!xml_found) {
		qInfo() << xmlPath << "NOT found...";
		return QVector<QSharedPointer<TextLine>>();
	}

	QSharedPointer<PageElement> xmlPage = parser.page();
	//QVector<QSharedPointer<TextLine>> textLineRegions = RegionManager::filter<TextLine>(xmlPage->rootRegion(), Region::type_text_line);
	QVector<QSharedPointer<TextLine>> textLineRegions = RegionManager::filter<TextLine>(xmlPage->rootRegion(), Region::type_word);

	return textLineRegions;
}

 QVector<QSharedPointer<TextPatch>> FontStyleClassificationTest::regionsToTextPatches(QVector<QSharedPointer<TextRegion>> wordRegions, LabelManager lm, cv::Mat img) {

	QVector<QSharedPointer<TextPatch>> textPatches = QVector<QSharedPointer<TextPatch>>();

	if (wordRegions.isEmpty()) {
		qWarning() << "No regions to convert here.";
		return textPatches;
	}

	for (auto wr : wordRegions) {
		Rect trRect = Rect::fromPoints(wr->polygon().toPoints());
		cv::Mat patchTexture = img(trRect.toCvRect());
		
		QSharedPointer<TextPatch> tp = QSharedPointer<TextPatch>::create(img, patchTexture);

		LabelInfo trLabel =  lm.find(wr->custom());
		
		if(!trLabel.isNull())
			tp->label()->setTrueLabel(trLabel);

		tp->setPolygon(wr->polygon());
		textPatches << tp;
	}
	return textPatches;
}

  QStringList FontStyleClassificationTest::loadTextSamples(QString filePath) {

	QFile textFile(filePath);
	if (!textFile.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open file containing text samples (words).";
		qInfo() << "Please provide a file (csv) containing samples.";
		return QStringList();
	}

	QStringList wordList;
	while (!textFile.atEnd()) {
		QString line = textFile.readLine();
		wordList.append(line.trimmed().split(','));
	}

	return wordList;
}

QSharedPointer<FontStyleClassifier> FontStyleClassificationTest::trainFontStyleClassifier(QString trainDir, GaborFilterBank gfb, int patchSize, int textureSize,
	FontStyleClassifier::ClassifierMode cm, int k, QString outputDir, bool saveToFile){

	//TODO make sure GT line/word regions are horizontal -> perform skew correction if necessary

	//load or generate train data set from file
	if (outputDir.isEmpty())
		outputDir = trainDir;

	QString trainDataSetOutputPath = QFileInfo(outputDir, "FontStyleDataSet_train.txt").absoluteFilePath();
	FeatureCollectionManager fcm_train;

	if (!QFileInfo(trainDataSetOutputPath).exists())
		FontDataGenerator::generateDataSet(trainDir, gfb, patchSize, textureSize, trainDataSetOutputPath);
	else {
		qInfo() << "Loading existing training data set: " << trainDataSetOutputPath;
		qInfo() << "Delete file to recompute data set.";
	}

	FontStyleDataSet fsd = FontStyleDataSet::read(trainDataSetOutputPath);
	fcm_train = fsd.featureCollectionManager();	

	if (fcm_train.isEmpty()) {
		qCritical() << "No training data found! Check training directory/data.";
		return QSharedPointer<FontStyleClassifier>::create();
	}

	//train classifier (and save it to file)
	auto fst = FontStyleTrainer(FontStyleDataSet(fcm_train, patchSize, gfb));
	
	auto fstConfig = fst.config();
	fstConfig->setModelType(cm);
	fstConfig->setDefaultK(k);

	if (!fst.compute())
		return QSharedPointer<FontStyleClassifier>::create();

	QSharedPointer<FontStyleClassifier> fsClassifier = fst.classifier();

	if(saveToFile) {
		
		if (outputDir.isEmpty())
			outputDir = trainDir;

		QString classifierFilePath;
		classifierFilePath = QFileInfo(outputDir, "FontStyleClassifier.txt").absoluteFilePath();

		if (!fsClassifier->write(classifierFilePath))
			qWarning() << "Failed to save font style classifier to file path: " << classifierFilePath;
	}

	return fsClassifier;
}

bool FontStyleClassificationTest::generateSnytheticTestPage(QString filePath, QString outputFilePath, QVector<QFont> synthPageFonts) {

	//read in text file and create QDocument from it
	QString inputText = readTextFromFile(filePath);

	if (inputText.isEmpty())
		return false;

	if (synthPageFonts.isEmpty()) {
		qWarning() << "Missing fonts for generation of synthetic test page.";
		return false;
	}

	//create document for rendering synthetic page image
	QTextDocument doc;
	QTextCursor cursor = QTextCursor(&doc);
	cursor.insertText(inputText);

	//apply random font styles
	int fIdx = QRandomGenerator::global()->bounded(0, synthPageFonts.size());
	int fCount = QRandomGenerator::global()->bounded(1, 16);
	QTextCharFormat textFormat;
	textFormat.setFont(synthPageFonts[fIdx]);

	cursor.movePosition(QTextCursor::Start);
	while (!cursor.atEnd()) {
		cursor.select(QTextCursor::WordUnderCursor);

		if (!cursor.selectedText().isEmpty()) {
			if (fCount == 0) {
				fIdx = QRandomGenerator::global()->bounded(0, synthPageFonts.size());
				fCount = QRandomGenerator::global()->bounded(1, 16);
				textFormat.setFont(synthPageFonts[fIdx]);
			}
			cursor.setCharFormat(textFormat);
			--fCount;
		}
		cursor.movePosition(QTextCursor::NextWord);
	}

	//draw synthetic text page
	doc.documentLayout();
	QImage qImg = QImage(doc.size().toSize(), QImage::Format_ARGB32);
	qImg.fill(QColor(255, 255, 255, 255)); //white background

	QPainter p(&qImg);
	doc.drawContents(&p);

	cv::Mat synthPageImg  = Image::qImage2Mat(qImg);

	//save page image 
	if (Image::save(synthPageImg, outputFilePath)) {
		if (!generateGroundTruthData(doc, outputFilePath))
			qWarning() << "Failed to generate ground truth file for synthetic page.";

		qInfo() << "Synthetic page image saved to: " << outputFilePath;
	}
	else{
		qWarning() << "Failed to save synthetic page image."; 
		return false;
	}

	return true;
}

bool FontStyleClassificationTest::generateGroundTruthData(QTextDocument& doc, QString filePath){

	QVector<QSharedPointer<TextRegion>> mTextLineRegions;
	QVector<QSharedPointer<Region>> textLineRegions;

	QTextCursor cursor = QTextCursor(&doc);
	cursor.movePosition(QTextCursor::Start);
	cursor.select(QTextCursor::WordUnderCursor); //LineUnderCursor | BlockUnderCursor

	// find bounding boxes of each word
	QTextBlock textBlock = cursor.block();
	QTextLine textLine = textBlock.layout()->lineForTextPosition(cursor.positionInBlock());
	QTextFrameFormat rootFrameFormat = doc.rootFrame()->frameFormat();

	while (!cursor.atEnd()) {

		//process QTextBlock
		if (textBlock.isValid()) {
			textBlock = cursor.block();
			
			//compute text line bounding box
			textLine = textBlock.layout()->lineForTextPosition(cursor.positionInBlock());
			QRectF blockBoundingRect = doc.documentLayout()->blockBoundingRect(textBlock);
			QSharedPointer<TextLine> textLineRegion = QSharedPointer<TextLine>::create();

			//process QTextLine
			if (textLine.isValid()) {
				QRectF croppedLineBox; //bounding box of line area covered by words (cropping trailing or leading white spaces)
				while (cursor.positionInBlock() <= textLine.textLength() && !cursor.atEnd()) {
					QString wordString = cursor.selectedText();

					int wordStartIdx = cursor.selectionStart();
					int wordEndIdx = cursor.selectionEnd();

					cursor.setPosition(wordStartIdx);
					wordStartIdx = cursor.positionInBlock();

					cursor.setPosition(wordEndIdx);
					wordEndIdx = cursor.positionInBlock();

					float flY = textLine.lineNumber() * textLine.height() + blockBoundingRect.top();
					float flX = textLine.cursorToX(wordStartIdx) + rootFrameFormat.leftMargin() + rootFrameFormat.padding();
					float flW = textLine.cursorToX(wordEndIdx) + rootFrameFormat.leftMargin() + rootFrameFormat.padding() - flX;
					float flH = textLine.height();

					QRectF wordBoundingBox = QRectF(flX, flY, flW, flH);

					if (wordBoundingBox.width() > 0) {
						if (croppedLineBox.isEmpty())
							croppedLineBox = wordBoundingBox;
						else
							croppedLineBox = croppedLineBox.united(wordBoundingBox);

						QSharedPointer<TextRegion> wordRegion = QSharedPointer<TextRegion>::create();
						QString fontLabel = FontStyleClassification::fontToLabelName(cursor.charFormat().font());

						wordRegion->setPolygon(Polygon::fromRect(Rect(wordBoundingBox)));
						wordRegion->setText(wordString);
						wordRegion->setCustom(fontLabel);

						textLineRegion->addChild(wordRegion);
					}	

					cursor.movePosition(QTextCursor::NextWord);
					cursor.select(QTextCursor::WordUnderCursor); //LineUnderCursor | BlockUnderCursor

					if (textBlock.blockNumber() != cursor.blockNumber())
						break;
				}

				if (!textLineRegion->children().isEmpty()) {

					//set final text line polygon
					QRectF textLineBoundingRect = textLine.rect();
					textLineBoundingRect.translate(blockBoundingRect.topLeft());
					textLineBoundingRect.setLeft(croppedLineBox.left());
					textLineBoundingRect.setRight(croppedLineBox.right());
					textLineRegion->setPolygon(Polygon::fromRect(Rect(textLineBoundingRect)));

					//set final baseline of text line
					int baseLineHeight = (int)textLineBoundingRect.top() + (int)textLine.ascent();
					QLine baseLine = QLine((int)textLineBoundingRect.left() + 1, baseLineHeight, (int)textLineBoundingRect.right() - 1, baseLineHeight);
					textLineRegion->setBaseLine(Line(baseLine));

					textLineRegions << textLineRegion;
				}
			}
			else {
				cursor.movePosition(QTextCursor::NextWord);
				cursor.select(QTextCursor::WordUnderCursor); //LineUnderCursor | BlockUnderCursor
			}
		}
		else {
			cursor.movePosition(QTextCursor::NextWord);
			cursor.select(QTextCursor::WordUnderCursor); //LineUnderCursor | BlockUnderCursor
		}
	}

	//write found regions to xml
	QString xmlPath;
	rdf::PageXmlParser parser;
	bool xml_found;
	QSharedPointer<PageElement> xmlPage;

	xmlPath = rdf::PageXmlParser::imagePathToXmlPath(filePath);
	xml_found = parser.read(xmlPath);

	// set up xml page
	xmlPage = parser.page();
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(doc.size().toSize());
	xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	//add results to xml
	xmlPage = parser.page();
	xmlPage->rootRegion()->removeAllChildren();

	for (auto tr : textLineRegions) {
		xmlPage->rootRegion()->addChild(tr);
	}

	parser.write(xmlPath, xmlPage);

	return true;
}

double FontStyleClassificationTest::computePrecision(const QVector<QSharedPointer<TextPatch>>& textPatches) const{
	
	int tp = 0;
	for (auto p : textPatches) {
		auto l = p->label();
		if (l->predicted() == l->trueLabel())
			tp++;
	}

	double precision = tp / (double)textPatches.size();

	return precision;
}

double FontStyleClassificationTest::evalTextPatchResults(const QVector<QSharedPointer<TextPatch>>& textPatches, 
	const LabelManager labelManager, QString outputDir, QString identifier) const {

	if (textPatches.isEmpty()) {
		qWarning() << "No text patches found! Could not compute evaluation results.";
		return 0;
	}
	
	QString evalOutput;

	//compute confusion matrix for classification results
	int maxID = 0;
	QString outputHeader = "\t";
	for(auto label : labelManager.labelInfos()){
		outputHeader += "; \tID " + QString::number(label.id()) + "\t";
		if (maxID < label.id())
			maxID = label.id();
	}
	
	QMap<int, QMap<int, int>> confMat;
	for (int i = 0; i <= maxID; i++) {
		for (int j = 0; j <= maxID; j++)
			confMat[i][j] = 0;
	}

	for (auto tp : textPatches) {
		int predID = tp->label()->predicted().id();
		int labelID = tp->label()->trueLabel().id();
		confMat[predID][labelID] += 1;
	}

	for (int i = 0; i <= maxID; i++) {
		int sum1 = 0;
		int sum2 = 0;
		for (int j = 0; j <= maxID; j++) {
			sum1 += confMat[i][j];
			sum2 += confMat[j][i];
		}
		confMat[i][maxID+1] = sum1;
		confMat[maxID + 1][i] = sum2;
	}

	//generate confusion matrix output
	evalOutput += outputHeader + "\n";
	for (int i = 0; i <= maxID+1; i++) {	
		QString outputRow;
		for (int j = 0; j <= maxID+1; j++) {

			if (outputRow.isEmpty())
				outputRow += "ID = " + QString::number(i) + "\t";
	
			double relPercentage = confMat[maxID + 1][j] != 0 ? ((double)confMat[i][j] / confMat[maxID + 1][j]) * 100.0 : 0;
			outputRow += ";\t" + QString::number(confMat[i][j]) + "(" + QString::number(relPercentage, 'f', 1) + "%)";
		}
		evalOutput += outputRow + "\n";
	}
	evalOutput += "\n";

	//generate class result output
	double overallPrecision = 0;
	for (int i = 1; i <= maxID; i++) {
		int idx = labelManager.indexOf(i);
		QString labelName = labelManager.labelInfos()[idx].toString();

		double precision = confMat[maxID + 1][i] != 0 ? ((double)confMat[i][i] / confMat[maxID + 1][i]) * 100.0 : 0;
		overallPrecision += precision;

		QString classOutput = "Precision for class " + labelName + " = " + QString::number(precision) +
			" (using " + QString::number(confMat[maxID + 1][i]) + " samples)";

		evalOutput += classOutput + "\n";
	}

	overallPrecision = overallPrecision / (double)maxID;
	evalOutput += "Overall classification precision = " + QString::number(overallPrecision) + "\n";

	qInfo().noquote() << evalOutput;
	
	//write output to file
	writeEvalResults(evalOutput, outputDir, identifier);

	return overallPrecision;
}

void FontStyleClassificationTest::writeEvalResults(QString evalSummary, QString outputDir, QString identifier) const{
	
	QFileInfo outDirInfo = QFileInfo(outputDir);
	if (!QFileInfo(outputDir).isDir())
		outputDir = outDirInfo.absolutePath();

	if (QFileInfo(outputDir).exists()) {

		QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
		QString output = "Font style classification results";

		if (!identifier.isEmpty())
			output += " for " + identifier;

		output += " from " + timeStamp + "\n\n" + evalSummary + "\n\n";

		QString outputFilePath = QFileInfo(outputDir, "FontStyleEvaluationResults.txt").absoluteFilePath();
		qDebug() << "outputFilePath: " << outputFilePath;

		QFile outputFile(outputFilePath);

		if (outputFile.open(QFile::WriteOnly | QIODevice::Text | QIODevice::Append)) {
			QTextStream outStream(&outputFile);
			outStream << output << endl;
			outStream.flush();
			outputFile.close();

			qInfo() << "Saved evaluation results to file:" << outputFilePath;
		}
		else
			qWarning() << "Could not open file for saving evaluation results: " << outputFilePath;
	}
	else
		qWarning() << "Ouput directory does not exist. Can not write results to dir: " + outputDir;
}

void FontStyleClassificationTest::appendOverallPerformance(double performance, QString outputFilePath, QString runIdentifier, QString addInfo) const{

	//append results to text file for easier comparison of multiple runs
	QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
	QString evalSummary = QString::number(performance).replace(".",",") + "%;" + runIdentifier + ";" + timeStamp + ";" + addInfo;

	QFile outputFile(outputFilePath);

	if (outputFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
		QTextStream outStream(&outputFile);
		outStream << evalSummary << endl;
		outStream.flush();
		outputFile.close();

		qInfo() << "Saved evaluation results to file:" << outputFilePath;
	}

}

//Text Block Formation Test ------------------------------------------------------------------------------
TextBlockFormationTest::TextBlockFormationTest(const DebugConfig & config){
	mConfig = config;
}

void TextBlockFormationTest::run(){
	//compute TBF results for training data set
		//get text lines for GT XML files
		//compute TB polygon for GT line regions
		//write TBF results to file
	//read in gt TB polygon
	//draw results of polygon against GT polygon for visual comparison
	
	QImage qImg(mConfig.imagePath());
	cv::Mat imgCv = Image::qImage2Mat(qImg);
	
	if(qImg.isNull())
		qInfo() << mConfig.imagePath() << "NOT loaded...";

	if (imgCv.empty()) {
		qInfo() << mConfig.imagePath() << "NOT loaded...";
		return;
	}

	QVector<QSharedPointer<TextLine>> textRegions = FontDataGenerator::loadRegions<TextLine>(mConfig.imagePath(), Region::type_text_line);

	double x = 661;
	double y = 1799;
	double w = 1056;
	double h = 92;
	Rect r1 = Rect(x - (w / 2), y - (h / 2), w, h);

	//merge lines :
	//621, 869; size(1053, 1201)
	//	605, 224; size(1056, 90)
	//	639, 1571; size(1056, 194)
	//	661, 1748; size(1056, 194)

	//	merge line comps :
	//661, 1799; size(1056, 92)

	mergeLineRegions(textRegions, r1);

	x = 621;
	y = 869;
	w = 1053;
	h = 1201;
	Rect r2 = Rect(x - (w / 2), y - (h / 2), w, h);

	x = 605;
	y = 224;
	w = 1056;
	h = 90;
	Rect r3 = Rect(x - (w / 2), y - (h / 2), w, h);

	x = 639;
	y = 1571;
	w = 1056;
	h = 194;
	Rect r4 = Rect(x - (w / 2), y - (h / 2), w, h);

	x = 661;
	y = 1748;
	w = 1056;
	h = 194;
	Rect r5 = Rect(x - (w / 2), y - (h / 2), w, h);

	QVector<Rect> blockRects;
	blockRects << r2 << r3 << r4 << r5;
	QVector<QSharedPointer<TextRegion>> textBlocks;

	for (auto tr : blockRects) {
		auto tb = mergeLinesToBlock(textRegions, tr);
		textBlocks << tb;
	}

	//create output xml
	rdf::PageXmlParser parser;
	QSharedPointer<PageElement> xmlPage;

	QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(mConfig.imagePath());
	xmlPath = Utils::createFilePath(xmlPath, "_tbf_eval");
	bool xml_found = parser.read(xmlPath);

	// set up xml page
	xmlPage = parser.page();
	xmlPage->setCreator(QString("CVL"));
	xmlPage->setImageSize(QSize(qImg.size()));
	xmlPage->setImageFileName(QFileInfo(xmlPath).fileName());

	xmlPage->rootRegion()->removeAllChildren();
	for (auto tb : textBlocks) {
		xmlPage->rootRegion()->addChild(tb);
	}
	parser.write(xmlPath, xmlPage);
}

void TextBlockFormationTest::mergeLineRegions(QVector<QSharedPointer<TextLine>> &textLines, Rect mergeRect){

	QVector<QSharedPointer<TextLine>> group;
	QVector<QSharedPointer<TextLine >> removeLines;
	QVector<Vector2D> pts;

	for (auto tr : textLines) {
		if (mergeRect.isNull() || mergeRect.contains(Rect::fromPoints(tr->polygon().toPoints()))) {
			pts << tr->polygon().toPoints();
			removeLines << tr;
		}
	}

	std::vector<cv::Point> ptsCv;
	for (const Vector2D& pt : pts) {
		ptsCv.push_back(pt.toCvPoint());
	}

	Polygon poly;
	if (pts.empty())
		poly = Polygon();
	else {
		std::vector<cv::Point> cPts;
		cv::convexHull(ptsCv, cPts, true);
		poly = Polygon::fromCvPoints(cPts);
	}

	for (auto tr : removeLines) {
		textLines.remove(textLines.indexOf(tr));
	}

	QSharedPointer<TextLine> newLine = QSharedPointer<TextLine>::create();
	newLine->setPolygon(Polygon(poly));
	textLines << newLine;

	//QImage qImg = QImage(mConfig.imagePath());
	//cv::Mat imgCv_ = Image::qImage2Mat(qImg);
	//QPainter painter(&qImg);

	//for (auto tr : textLines) {
	//	tr->polygon().draw(painter);
	//}

	//cv::Mat textRegionImg = Image::qImage2Mat(qImg);

}

QSharedPointer<TextRegion> TextBlockFormationTest::mergeLinesToBlock(QVector<QSharedPointer<TextLine>> textLines, Rect mergRect){

	QVector<QSharedPointer<TextLine>> tbLines;

	for(auto tr : textLines) {
		if (mergRect.contains(Rect::fromPoints(tr->polygon().toPoints())))
			tbLines << tr;
	}

	Polygon tbPoly = computeTextBlockPolygon(tbLines);
	QSharedPointer<TextRegion> tb = QSharedPointer<TextRegion>::create();
	tb->setPolygon(tbPoly);

	return tb;
}

Polygon TextBlockFormationTest::computeTextBlockPolygon(QVector<QSharedPointer<TextLine>> textLines){

	if (textLines.isEmpty())
		return Polygon();

	QPolygonF poly;

	bool fitPoly = false;
	bool convexPoly = false;
	bool boxPoly = true;

	if (fitPoly) {
		Rect bbox = Rect(0, 0, 1, 1);
		for (auto tl : textLines)
			bbox = bbox.joined(Rect::fromPoints(tl->polygon().toPoints()));

		QImage qPolyImg((int)bbox.width(), (int)bbox.height(), QImage::Format_RGB888);
		qPolyImg.fill(QColor(0, 0, 0));
		QPainter painter(&qPolyImg);
		painter.setPen(QColor(255, 255, 255));
		painter.setBrush(QColor(255, 255, 255));

		for (int i = 0; i < textLines.length(); i++) {

			auto l1 = textLines[i];

			if (i == 0) {
				poly = l1->polygon().polygon();
				painter.drawPolygon(poly);
			}
			else if (i > 0) {

				auto l2 = l1;
				l1 = textLines[i - 1];
				QVector<QSharedPointer<TextLine>> mergeLines;
				mergeLines << l1 << l2;
				mergeLineRegions(mergeLines);

				QPolygonF hull_poly = mergeLines[0]->polygon().polygon();

				auto poly1 = l1->polygon().polygon();
				auto poly2 = l2->polygon().polygon();

				//compute rect containing max left and min right bound of the text lines
				auto rect1 = poly1.boundingRect();
				auto rect2 = poly2.boundingRect();

				double top = std::min(rect1.top(), rect2.top());
				double left = std::max(rect1.left(), rect2.left());
				double bottom = std::max(rect1.bottom(), rect2.bottom());
				double right = std::min(rect1.right(), rect2.right());

				Rect rect = Rect(left, top, right - left, bottom - top);
				QPolygonF rect_poly = Polygon::fromRect(rect).polygon();

				auto cut_poly = hull_poly.intersected(rect_poly);

				painter.drawPolygon(poly2);
				painter.drawPolygon(cut_poly);
			}
		}

		cv::Mat polyImg = Image::qImage2Mat(qPolyImg);
		cvtColor(polyImg, polyImg, cv::COLOR_RGB2GRAY);

		std::vector<std::vector<cv::Point>> contours;
		cv::findContours(polyImg, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		//debug
		//cv::Mat contour_img = cv::Mat::zeros(mImg.size().height, mImg.size().width, CV_8UC1);
		//cv::fillPoly(contour_img, contours, 255, 8);

		if (contours.size() > 1)
			qWarning() << "Found more than one contour element for a single text block. Using only the first one.";

		poly = Polygon::fromCvPoints(contours[0]).polygon();
	}

	else if (convexPoly) {
		mergeLineRegions(textLines);
		poly = textLines[0]->polygon().polygon();
	}
	else if (boxPoly) {
		mergeLineRegions(textLines);
		Rect box = Rect::fromPoints(textLines[0]->polygon().toPoints());
		poly = Polygon::fromRect(box).polygon();
	}

	return poly;
}

}