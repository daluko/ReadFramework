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


#pragma warning(push, 0)	// no warnings from includes
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QImage>
#include <QFileInfo>
#pragma warning(pop)

#include "Utils.h"
#include "PieData.h"
#include "Settings.h"
#include "DebugUtils.h"
#include "DebugMarkus.h"
#include "DebugFlo.h"
#include "DebugStefan.h"
#include "DebugThomas.h"
#include "DebugDavid.h"
#include "PageParser.h"
#include "Shapes.h"

#if defined(_MSC_BUILD) && !defined(QT_NO_DEBUG_OUTPUT) // fixes cmake bug - really release uses subsystem windows, debug and release subsystem console
#pragma comment (linker, "/SUBSYSTEM:CONSOLE")
#else
#pragma comment (linker, "/SUBSYSTEM:WINDOWS")
#endif

void applyDebugSettings(rdf::DebugConfig& dc);
bool testFunction();

int main(int argc, char** argv) {

	// check opencv version
	qInfo().nospace() << "I am using OpenCV " << CV_MAJOR_VERSION << "." << CV_MINOR_VERSION << "." << CV_VERSION_REVISION;

	QCoreApplication::setOrganizationName("TU Wien");
	QCoreApplication::setOrganizationDomain("https://cvl.tuwien.ac.at/");
	QCoreApplication::setApplicationName("READ Framework");
	rdf::Utils::instance().initFramework();

	QGuiApplication app(argc, (char**)argv);	// enable headless

	// CMD parser --------------------------------------------------------------------
	QCommandLineParser parser;

	parser.setApplicationDescription("Welcome to the CVL READ Framework testing application.");
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addPositionalArgument("imagepath", QObject::tr("Path to an input image."));

	// xml path
	QCommandLineOption xmlOpt(QStringList() << "x" << "xml", QObject::tr("Path to PAGE xml. If provided, we make use of the information"), "path");
	parser.addOption(xmlOpt);

	// output image
	QCommandLineOption outputOpt(QStringList() << "o" << "output", QObject::tr("Path to output image."), "path");
	parser.addOption(outputOpt);

	// developer
	QCommandLineOption modeOpt(QStringList() << "m" << "mode", QObject::tr("Mode defines the methodology. For Baseline detection use [-m layout]"), "name");
	parser.addOption(modeOpt);

	// settings filename
	QCommandLineOption settingOpt(QStringList() << "s" << "setting", QObject::tr("Settings filepath."), "filepath");
	parser.addOption(settingOpt);

	// settings classifier
	QCommandLineOption classifierOpt(QStringList() << "c" << "classifier", QObject::tr("Classifier file path."), "filepath");
	parser.addOption(classifierOpt);

	// feature cache path
	QCommandLineOption featureCachePathOpt(QStringList() << "f" << "feature cache", QObject::tr("Feature cache path for training."), "filepath");
	parser.addOption(featureCachePathOpt);

	// label config path
	QCommandLineOption labelConfigPathOpt(QStringList() << "l" << "label config", QObject::tr("Label config path for training."), "filepath");
	parser.addOption(labelConfigPathOpt);

	// table template path
	QCommandLineOption xmlTableOpt(QStringList() << "t" << "table template", QObject::tr("Path to PAGE xml of table template. Table must be specified"), "templatepath");
	parser.addOption(xmlTableOpt);

	// font training data path
	QCommandLineOption fontDataOpt(QStringList() << "w" << "font data", QObject::tr("Path to directory containing training data for font style classification."), "fontdata");
	parser.addOption(fontDataOpt);

	// pie db path
	QCommandLineOption jsonOpt(QStringList() << "json", QObject::tr("Path to JSON file for PIE crawler"), "filepath");
	parser.addOption(jsonOpt);

	parser.process(*QCoreApplication::instance());
	// CMD parser --------------------------------------------------------------------

	// stop processing if little tests are preformed
	if (testFunction())
		return 0;

	// load settings
	rdf::Config& config = rdf::Config::instance();
	rdf::FormFeaturesConfig fc;

	// load user defined settings
	if (parser.isSet(settingOpt)) {
		QString sName = parser.value(settingOpt);
		config.setSettingsFile(sName);
		config.load();

		QSettings s(sName, QSettings::IniFormat);
		s.beginGroup("FormAnalysis");
		fc.loadSettings(s);
		s.endGroup();
	}

	// create debug config
	rdf::DebugConfig dc;
	
	if (parser.positionalArguments().size() > 0)
		dc.setImagePath(parser.positionalArguments()[0].trimmed());

	// add output path	
	if (parser.isSet(outputOpt))
		dc.setOutputPath(parser.value(outputOpt));

	// add xml path	
	if (parser.isSet(xmlOpt))
		dc.setXmlPath(parser.value(xmlOpt));

	// add classifier path	
	if (parser.isSet(classifierOpt))
		dc.setClassifierPath(parser.value(classifierOpt));

	// add feature cache path
	if (parser.isSet(featureCachePathOpt))
		dc.setFeatureCachePath(parser.value(featureCachePathOpt));

	// add label config path
	if (parser.isSet(labelConfigPathOpt))
		dc.setLabelConfigPath(parser.value(labelConfigPathOpt));

	// add table template
	if (parser.isSet(xmlTableOpt)) {
		dc.setTableTemplate(parser.value(xmlTableOpt));
	} else if (parser.isSet(settingOpt)) {
		dc.setTableTemplate(fc.templDatabase());
	}

	// add font data path
	if (parser.isSet(fontDataOpt))
		dc.setFontDataPath(parser.value(fontDataOpt));

	// apply debug settings - convenience if you don't want to always change the cmd args
	applyDebugSettings(dc);

	//rdf::XmlTest xmlTest(dc);
	//xmlTest.parseXml();
	
	if (!dc.imagePath().isEmpty()) {

		// flos section
		if (parser.isSet(modeOpt) && parser.value(modeOpt) == "binarization") {
			// TODO do what ever you want
			qDebug() << "starting binarization ...";
			rdf::BinarizationTest test(dc);
			test.binarizeTest();
		}
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "table") {
			qDebug() << "starting table matching ...";
			//TODO table
			rdf::TableProcessing tableproc(dc);
			tableproc.setTableConfig(fc);
			tableproc.match();
		}
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "atable") {
			qDebug() << "applying table ...";
			//TODO table
			rdf::TableProcessing tableproc(dc);
			tableproc.setTableConfig(fc);
			tableproc.apply();
		}
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "pie") {

			QString jsonPath;
			if (parser.isSet(jsonOpt))
				jsonPath = parser.value(jsonOpt);
			
			rdf::PieData testDB(dc.imagePath(), jsonPath);
			testDB.saveJsonDatabase();
		}
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "separators") {
			//TODO just calculate separators (visual lines) and write to xml
			qDebug() << "starting line extraction ...";
			rdf::LineProcessing lineproc(dc);
			lineproc.lineTrace();
		}
		// stefans section
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "wi") {
			qDebug() << "starting writer retrieval ...";

			rdf::TestWriterRetrieval twr = rdf::TestWriterRetrieval();
			twr.run();
		}
		// layout section
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "layout") {
			qDebug() << "Starting layout analysis ...";

			rdf::LayoutTest lt(dc);
			lt.layoutToXml();
		}
		// thomas
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "apa") {
			qDebug() << "Starting newspaper analysis ...";
			rdf::ThomasTest test(dc);
			test.test();
		}
		// davids section
		else if (parser.isSet(modeOpt) && parser.value(modeOpt) == "david") {

			qDebug() << "loading david's debug code";

			QString dirPath;
			//dirPath = "F:/dev/da/data/HBR2013/test/tr";
			//dirPath = "E:/data/test/HBR13_test";
			//dirPath = "F:/dev/da/data/HBR2013/eval/train";
			dirPath = "F:/dev/da/data/HBR2013/eval/test";

			bool testTHE = false;
			bool testWSA = false;
			bool testFSC = true;
			bool testTBF = false;

			if (testTBF) {
				rdf::TextBlockFormationTest tbft(dc);
				tbft.run();
			}

			if (testTHE) {
				rdf::TextHeightEstimationTest thet(dc);
				//thet.drawTextHeightRect(QRect(0,0,75,75));
				////EGBA -> x > 50 -> ~60-75

				if (!dirPath.isEmpty())
					thet.processDirectory(dirPath);
				else {
					thet.run();
				}
			}

			if (testWSA) {	
				//dirPath = "F:/dev/da/data/catalogue/fsc_selection/1907_Brussels_EGBA/LA Results";
				dirPath = "F:/dev/da/data/catalogue/fsc_selection/1905_Venice_EI/LA Results";
				//dirPath = "F:/dev/da/data/catalogue/fsc_selection/1907_Paris_SdA/LA Results";

				if (!dirPath.isEmpty())
					dc.setOutputPath(dirPath);
				else
					dc.setOutputPath(dc.imagePath());

				rdf::WhiteSpaceTest wst(dc);

				//parameter tests
				//wst.testParameterSettings(dirPath);
				//wst.testFontHeightRatio();

				//process directory
				if (!dirPath.isEmpty())
					wst.processDirectory(dirPath);
				else {
					wst.run();
				}
			}

			if (testFSC) {
				rdf::FontStyleClassificationTest fct(dc);

				//if (!dirPath.isEmpty())
				//	fct.processDirectory(dirPath);
				//else
				//	fct.run();

				if (!dc.fontDataPath().isEmpty()) {

					//test text patch processing			
					//QString testPath = dc.fontDataPath() + "/gabor param test/";
					//qDebug() << testPath;
					////fct.testSyntheticDataSet(testPath, 500);
					//fct.testSyntheticDataSet(testPath);

					//test limited sample count on text synthetic patches
					//TODO write results of multiple runs in a single file for easier evaluation
					//int maxSampleCount = 25;
					//while(maxSampleCount <= 3600) {
					//	fct.testSyntheticDataSet(testPath, maxSampleCount);
					//	maxSampleCount = maxSampleCount + 25;
					//}

					//test synthetic page processing
					//QString pageDataPath = dc.fontDataPath() + "syntheticPage/pageData.txt";
					//QString trainDataPath = dc.fontDataPath() + "syntheticPage/FontTrainData.txt";
					//
					//qDebug() << "synthPage input path 1: " << pageDataPath;
					//qDebug() << "synthPage input path 2: " << trainDataPath;

					//fct.testSyntheticPage(pageDataPath, trainDataPath);

					//test word regions of catalogue data 
					QString dataDir = "F:/dev/da/data/catalogue/fsc_selection/1907_Brussels_EGBA";
					//QString dataDir = "F:/dev/da/data/catalogue/fsc_selection/1907_Paris_SdA";
					//QString dataDir = "F:/dev/da/data/catalogue/fsc_selection/1905_Venice_EI";
					fct.testCatalogueRegions(dataDir);
				}
				else {
					qWarning() << "Can't test font style classification with synthetic data.";
					qInfo() << "Use -w option to specify path to .txt file containing text samples (words).";
				}
			}
		}
		// my section
		else {

			qDebug() << "hey markus - your section is empty...";
			//rdf::XmlTest test(dc);
			//test.parseXml();
			//test.linesToXml();

			//rdf::LayoutTest lt(dc);
			//lt.testComponents();

			//rdf::DeepMergeTest dm(dc);
			//dm.run();
		}

	}
	else {
		qInfo() << "Please specify an input image...";
		parser.showHelp();
	}

	// save settings
	config.save();
	return 0;	// thanks
}

void applyDebugSettings(rdf::DebugConfig& dc) {

	if (dc.imagePath().isEmpty()) {

		dc.setImagePath("C:/read/test/sizes/synthetic-test-small.png");
		dc.setImagePath("C:/read/test/sizes/synthetic-test.png");
		dc.setImagePath("C:/read/test/d6.5/0056_S_Alzgern_011-01_0056-crop.JPG");
		dc.setImagePath("C:/read/baseline-evaluation/BL_English/Images/ior!p!241!37_8_feb_1793_pp_594-610_f001v.jpg");

		dc.setImagePath("C:/nextcloud/READ/basilis/merging/Mss_003357_0152_pag-047[051]-probs.png");
		//dc.setImagePath("C:/nextcloud/READ/basilis/merging/_5269957-probs.png");

		//dc.setImagePath("C:/read/test/sizes/synthetic-test-small.png");
		//dc.setImagePath("C:/read/test/sizes/synthetic-test.png");
		//dc.setImagePath("C:/read/test/d6.5/0056_S_Alzgern_011-01_0056-crop.JPG");
		//dc.setImagePath("C:/read/test/d6.5/eval/P_241_27_012.jpg");

		//dc.setImagePath("E:/data/test/HBR2013_training/00443033.tif");		
		//dc.setImagePath("E:/data/test/HBR2013_training/training/00452456.tif");
		//dc.setImagePath("E:/data/test/HBR2013_training/test/00046981.tif");

		//dc.setImagePath("F:/dev/da/data/catalogue/fsc_selection/1907_Brussels_EGBA/1907_Brussels_BeauxArts_0014.jpg");
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/train/00485679.tif");//athena example
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00058028.tif"); //black separator example
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00672923.tif"); //rnn example - add pixel, merge lines
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/train/00456592.tif"); //rnn example - merge lines
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00439450.tif"); //wss example
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/train/00456592.tif"); //mss example
		//dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00454868.tif"); //mss2 example
		dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00425629.tif"); //tbf1 example
		dc.setImagePath("F:/dev/da/data/HBR2013/eval/train/00465433.tif"); //tbf poly example
		dc.setImagePath("F:/dev/da/data/HBR2013/eval/test/00485661.tif");  //tbf poly clipping test
		//tbf example 00441830
		//tbf example 00486100 - debug
		

		qInfo() << dc.imagePath() << "added as image path";
	}

	if (dc.outputPath().isEmpty()) {
		dc.setOutputPath(rdf::Utils::createFilePath(dc.imagePath(), "-result", "png"));
		qInfo() << dc.outputPath() << "added as output path";
	}

	if (dc.classifierPath().isEmpty()) {
		dc.setClassifierPath("C:/read/configs/test/test-two-classes/test-model.json");
		qInfo() << dc.classifierPath() << "added as classifier path";
	} 

	if (dc.labelConfigPath().isEmpty()) {
		dc.setLabelConfigPath("C:/read/configs/test/test-two-classes/test-config.json");
		qInfo() << dc.labelConfigPath() << "added as label config path";
	} 

	if (dc.featureCachePath().isEmpty()) {
		dc.setFeatureCachePath("C:/read/configs/test/test-two-classes/test-features.json");
		qInfo() << dc.featureCachePath() << "added as feature cache path";
	} 

	if (dc.xmlPath().isEmpty()) {
		QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(dc.imagePath());
		dc.setXmlPath(xmlPath);		// overwrite
		//dc.setXmlPath(rdf::Utils::createFilePath(xmlPath, "-result"));

		//dc.setXmlPath("C:/temp/T_Aigen_am_Inn_001_0056.xml");
		qInfo() << dc.xmlPath() << "added as XML path";
	} 

	if (dc.fontDataPath().isEmpty()) {
		QString fontDataPath = "F:/dev/da/data/SynthFontData/";
		dc.setFontDataPath(fontDataPath);		// overwrite
		qInfo() << dc.fontDataPath() << "added as font data path";
	}

	// add your debug overwrites here...
}

bool testFunction() {

	// tests the line distance to point function
	//rdf::Vector2D l1(1916, 1427);
	//rdf::Vector2D l2(1931, 1859);
	//rdf::Vector2D l3(1915, 834);
	//rdf::Vector2D l4(1952, 3846);

	//rdf::Line l(l3,l4);

	//qDebug() << l.distance(l2) << "is the distance";

	return false;
}