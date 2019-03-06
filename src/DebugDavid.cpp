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
#include "WhiteSpaceAnalysis.h"
#include "TextHeightEstimation.h"
#include "FontStyleClassification.h"
#include "FontStyleTrainer.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
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

	//TODO consider using duplicate words for training, should be avoided for test
	//TODO create font style data set class and store test and train data set (+ text patches) in one file only
	//TODO refactoring

	//generate file path for saving train data
	QString fontDataDir = QFileInfo(filePath).absolutePath();
	QString rtPath = QFileInfo(fontDataDir, "FontTrainData_rt.txt").absoluteFilePath();
	QString trainDataSetPath = QFileInfo(fontDataDir, "FontStyleDataSet_train.txt").absoluteFilePath();
	QString testDataSetPath = QFileInfo(fontDataDir, "FontStyleDataSet_test.txt").absoluteFilePath();
	QString classifierPath = QFileInfo(fontDataDir, "FontStyleClassifier.txt").absoluteFilePath();

	//generate data sets (if files do not exist)
	if (!QFileInfo(trainDataSetPath).exists() && !QFileInfo(testDataSetPath).exists()) {

		QStringList wordList = generateSamplesFromTextFile(rtPath);

		if (wordList.isEmpty())
			return;

		QVector<QStringList> sampleSets = splitSampleSet(wordList);
		LabelManager lm = generateFontLabelManager();

		generateDataSet(sampleSets[0], lm, trainDataSetPath);
		generateDataSet(sampleSets[1], lm, testDataSetPath);
	}

	//load data sets
	QStringList samples_train, samples_test;
	FeatureCollectionManager fcm_train, fcm_test;

	if (!readDataSet(trainDataSetPath, fcm_train, samples_train))
		return;

	if (!readDataSet(testDataSetPath, fcm_test, samples_test))
		return;

	//generate test data
	auto labelManager = fcm_train.toLabelManager();
	QVector<QSharedPointer<TextPatch>> textPatches_test = generateTextPatches(samples_test, labelManager);

	//load or generate font style classifier
	QSharedPointer<FontStyleClassifier> fsClassifier;
	if (QFileInfo(classifierPath).exists()) {
		qInfo() << "Loading classifier from existing file: " << classifierPath;

		fsClassifier = FontStyleClassifier::read(classifierPath);
		if (fsClassifier->isEmpty()) {
			qCritical() << "Failed to load existing classifier from file.";
			qInfo()		<< "Delete existing file to generate a new one.";
			return;
		}
	}
	else{
		auto fst = FontStyleTrainer(fcm_train);

		////test different classfier modes
		//auto fstConfig = fst.config();
		//fstConfig->setModelType(FontStyleClassifier::classify_bayes);
		//fstConfig->setDefaultK(25);

		if (!fst.compute()) {
			qCritical() << "Failed to train font style classifier.";
			return;
		}

		fsClassifier = fst.classifier();
		fsClassifier->write(classifierPath);	//optional
	}

	//compute font style classification results
	FontStyleClassification fsc = FontStyleClassification(textPatches_test, testDataSetPath);
	fsc.setClassifier(fsClassifier);

	if (!fsc.compute()) {
		qCritical() << "Failed to compute font style classification results";
		return;
	}

	//compute evaluation results
	auto textPatches = fsc.textPatches();
	evalSyntheticDataResults(textPatches, labelManager, fontDataDir);

	qInfo() << "Finished font style classification test on synthetic data.";
}

bool FontStyleClassificationTest::generateDataSet(QStringList samples,
	LabelManager labelManager, QString outputFilePath) {

	QFileInfo outputFilePathInfo(outputFilePath);
	if (outputFilePathInfo.exists()) {
		qWarning() << "File already exists. Delete existing file to generate new one:" << outputFilePathInfo.absoluteFilePath();
		return false;
	}

	if (samples.isEmpty() || labelManager.isEmpty()) {
		qCritical() << "Could not generate data set, missing input data.";
		return false;
	}

	QVector<QSharedPointer<TextPatch>> textPatches = generateTextPatches(samples, labelManager);
	FeatureCollectionManager fcm = generatePatchFeatures(textPatches);

	//write data set to file
	QJsonObject jo = fcm.toJson(outputFilePath);

	QJsonArray ja = QJsonArray::fromStringList(samples);
	jo.insert("wordSamples", ja);

	Utils::writeJson(outputFilePath, jo);

	return true;
}

bool FontStyleClassificationTest::readDataSet(QString inputFilePath, FeatureCollectionManager& fcm, QStringList& samples) const{

	fcm = FeatureCollectionManager::read(inputFilePath);
	QJsonArray sampleJA = Utils::readJson(inputFilePath).value("wordSamples").toArray();

	if (sampleJA.isEmpty() || fcm.isEmpty()) {
		qCritical() << "Failed to load data set from: " << inputFilePath;
		return false;
	}

	for (auto s : sampleJA)
		samples << s.toString();

	qInfo() << "Successfully loaded data set from: " << inputFilePath;
	return true;
}

QStringList FontStyleClassificationTest::generateSamplesFromTextFile(QString filePath, int minWordLength, bool removeDuplicates) {

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		qInfo() << "Could not open file: " << filePath;
		return QStringList();
	}

	QTextStream inStream(&file);
	QString inputText = inStream.readAll();
	inStream.flush();
	file.close();

	if (inputText.isEmpty()) {
		qInfo() << "Text file is empty.";
		return QStringList();
	}

	//use alphanumeric symbols only 
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[-`~!@#$%^&*()_â€”+=|:;<>Â«Â»,.?/{}\'\"\\\[\\\]\\\\]")), " ");

	//remove numbers too (letters only)
	//inputText = inputText.replace(QRegExp(QString::fromUtf8("[0 - 9]")), " ");

	//remove formatting symbols
	inputText = inputText.replace(QRegExp(QString::fromUtf8("[\\n\\t\\r]")), " ");

	QStringList wordListTmp = inputText.split(' ');

	//remove short words
	QStringList wordList;
	for (auto word : wordListTmp) {
		if (word.length() >= minWordLength)	//use only words with 4+ characters
			wordList.append(word);
	}

	if(removeDuplicates)
		wordList.removeDuplicates();

	if (wordList.size() < 50) {
		qCritical() << "No data set created. Please provide a text file containing (>50) unique words.";
		qCritical() << "Each word should have a minimum of " << minWordLength << " characters.";
		return QStringList();
	}

	return wordList;
}

QVector<QStringList> FontStyleClassificationTest::splitSampleSet(QStringList sampleSet, double ratio) {

	if (sampleSet.isEmpty()) {
		qWarning() << "Sample set is empty.";
		return QVector<QStringList>();
	}

	int wNum = sampleSet.size();
	int s = (int)std::floor((double)wNum*0.8);

	QStringList wordListTrain = sampleSet.mid(0, s);
	QStringList wordListTest = sampleSet.mid(s);

	QVector<QStringList> splitSets = {wordListTrain, wordListTest};

	return splitSets;
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

QVector<QSharedPointer<TextPatch>> FontStyleClassificationTest::generateTextPatches(QStringList textSamples, LabelManager labelManager) {

	//TODO test parameters for size of text, size of text patches, size of text patch line height, etc.

	if (textSamples.isEmpty()) {
		qCritical() << "Found no text samples, could not compute text patches.";
		return QVector<QSharedPointer<TextPatch>>();
	}

	if (labelManager.isEmpty()) {
		qCritical() << "Failed to generate test patches.";
		qWarning() << "Label manager is empty.";
		return QVector<QSharedPointer<TextPatch>>();
	}

	//filter font style labels contained in label manager
	auto labels_ = labelManager.labelInfos();
	QVector<LabelInfo> fontStyleLabels;

	for (LabelInfo l : labels_) {
		if (l.name().startsWith("fsl_"))
			fontStyleLabels << l;
	}

	QVector<QSharedPointer<TextPatch>> textPatches;
	for (auto l : fontStyleLabels) {
		for (auto s : textSamples) {
			auto tp = QSharedPointer<TextPatch>::create(s, l);
			if (!tp->isEmpty())
				textPatches << tp;
		}
	}

	qInfo() << "Computed " << textPatches.size() << " text patches.";

	return textPatches;
}

FeatureCollectionManager FontStyleClassificationTest::generatePatchFeatures(QVector<QSharedPointer<TextPatch>> textPatches) {

	//generate feature collection for each training sample and font style
	GaborFilterBank gfb = FontStyleClassification::createGaborKernels();

	qInfo() << "Computing features for sample text patches. This might take a while...";
	cv::Mat tpFeatures = FontStyleClassification::computeGaborFeatures(textPatches, gfb);

	//save feature collection manager
	FeatureCollectionManager fcm = FontStyleClassification::generateFCM(textPatches, tpFeatures);
	
	return fcm;
}

LabelManager FontStyleClassificationTest::generateFontLabelManager() {

	LabelManager labelManager = LabelManager();
	QVector<QString> labelNames;
	QVector<LabelInfo> fontLabels;

	bool italic = false;
	int weight = QFont::Normal;
	int size = 30;
	QString fFamily = "Arial";
	QFont font(fFamily, size, weight, italic);

	QVector<QFont> fontStyleCollection;
	fontStyleCollection << font;
	
	font.setBold(true);
	fontStyleCollection << font;

	font.setBold(true);
	font.setItalic(true);
	fontStyleCollection << font;

	font.setBold(false);
	fontStyleCollection << font;

	for (int i = 0; i < fontStyleCollection.size(); i++) {
		QString labelName = FontStyleClassification::fontToLabelName(fontStyleCollection[i]);
		LabelInfo label(i + 1, labelName);

		labelNames << labelName;
		labelManager.add(label);
	}

	return labelManager;
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

void FontStyleClassificationTest::evalSyntheticDataResults(const QVector<QSharedPointer<TextPatch>>& textPatches, 
	const LabelManager labelManager, QString outputDir) const {

	if (textPatches.isEmpty()) {
		qWarning() << "No text patches found! Could not compute evaluation results.";
		return;
	}

	//group text patches belonging to the same gt class
	QMap<int, QVector<QSharedPointer<TextPatch>>> tpClasses;
	for (auto tp : textPatches) {
		auto labelID = tp->label()->trueLabel().id();

		if (!tpClasses.contains(labelID)) {
			QVector<QSharedPointer<TextPatch>> tpClass = { tp };
			tpClasses.insert(labelID, tpClass);
			continue;
		}

		auto tpClass = tpClasses.value(labelID);
		tpClass << tp;
		tpClasses.insert(labelID, tpClass);
	}

	//compute results for each gt class
	double overallPrecision = 0;
	QString evalOutput;

	QMap<int, double> tpClassResults;
	for (int labelID : tpClasses.keys()) {
		double precision = computePrecision(tpClasses.value(labelID));
		tpClassResults.insert(labelID, precision);
		
		overallPrecision += precision;
		
		int idx = labelManager.indexOf(labelID);
		QString labelName = labelManager.labelInfos()[idx].toString();

		QString outputString = "Precision for class " + labelName + " = " + QString::number(precision) +
			" (using " + QString::number(tpClasses.value(labelID).size()) + " samples)";

		qInfo() << outputString;
		evalOutput += outputString + "\n";
	}
	
	overallPrecision = overallPrecision / (double) tpClassResults.size();
	QString outputString = "Overall classification precision = " + QString::number(overallPrecision);
	
	qInfo() << outputString;
	evalOutput += "\n" + outputString;
	
	//write results to file
	if (!outputDir.isEmpty())
		writeEvalResults(evalOutput, outputDir);
	else
		return;
}

void FontStyleClassificationTest::writeEvalResults(QString evalSummary, QString outputDir) const{
	outputDir = QFileInfo(outputDir).absolutePath();
	if (QFileInfo(outputDir).exists()) {

		QString timeStamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm");
		evalSummary = "Font style classification results from " + timeStamp + "\n\n" + evalSummary;

		QString outputFilePath = QFileInfo(outputDir, "FontStyleEvaluationResults_" + timeStamp + ".txt").absoluteFilePath();
		QFile outputFile(outputFilePath);

		if (outputFile.open(QFile::WriteOnly | QIODevice::Text)) {
			QTextStream outStream(&outputFile);
			outStream << evalSummary << endl;
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

}