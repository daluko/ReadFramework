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

 You should have received a copy of the GNU General Public Licensex
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 The READ project  has  received  funding  from  the European  Union’s  Horizon  2020  
 research  and innovation programme under grant agreement No 674943
 
 related links:
 [1] http://www.caa.tuwien.ac.at/cvl/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] http://nomacs.org
 *******************************************************************************************************/

#pragma once

#include "DebugUtils.h"
#include "ScaleFactory.h"
#include "FontStyleClassification.h"
#include "Elements.h"
#include "WhiteSpaceAnalysis.h"

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#include <QTextDocument>
#pragma warning(pop)

// Qt/CV defines
namespace cv {
	class Mat;
}

namespace rdf {

class WhiteSpaceTest {
	
public:
	WhiteSpaceTest(const DebugConfig& config = DebugConfig());

	void run();
	void processDirectory(const QString dirPath);
	void testParameterSettings(const QString dirPath = QString());
	void testFontHeightRatio();


protected:
	DebugConfig mConfig;
	QSharedPointer<WhiteSpaceAnalysisConfig> mWsaConfig;
	QSharedPointer<TextLineHypothesizerConfig> mTlhConfig;
	QSharedPointer<WhiteSpaceSegmentationConfig> mWssConfig;
	QSharedPointer<TextBlockFormationConfig> mTbfConfig;
};

class FontStyleClassificationTest {

public:
	FontStyleClassificationTest(const DebugConfig& config = DebugConfig());

	void run();
	void processDirectory(const QString dirPath);
	void testSyntheticDataSet(QString filePath, int maxSampleCount = -1);
	void testSyntheticPage(QString filePath, QString trainDataPath);
	void testClassifierTraining(QString dirPath);

protected:
	QStringList loadTextSamples(QString filePath);
	QVector<QSharedPointer<TextLine>> loadTextLines(QString imagePath);
	QVector< QSharedPointer<TextRegion>> loadWordRegions(QString imagePath);
	template <typename T>
	QVector< QSharedPointer<T>> loadRegions(QString imagePath, Region::Type type);
	QVector<QSharedPointer<TextPatch>> regionsToTextPatches(QVector<QSharedPointer<TextRegion>> wordRegions, LabelManager lm,  cv::Mat img);

	bool readDataSet(QString inputFilePath, FeatureCollectionManager & fcm, QStringList & samples) const;
	void reduceSampleCount(FeatureCollectionManager & fcm, int sampleCount) const;

	QStringList loadWordSamples(QString filePath, int minWordLength = 4, bool removeDuplicates = true);
	QString readTextFromFile(QString filePath);
	QVector<QStringList> splitSampleSet(QStringList samplesSet, double ratio = 0.8);
	
	bool generateSnytheticTestPage(QString filePath, QString outputFilePath, QVector<QFont> synthPageFonts);
	bool generateGroundTruthData(QTextDocument& doc, QString filePath);
	bool generateDataSet(QStringList sample, QVector<QFont> fonts, QString outputFilePath);

	LabelManager generateFontLabelManager(QVector<QFont> fonts);
	QVector<QSharedPointer<TextPatch>> generateTextPatches(QStringList textSamples, LabelManager labelManager);
	QVector<QSharedPointer<TextPatch>> generateTextPatches(QVector<QSharedPointer<TextLine>> wordRegions, cv::Mat img, int PatchSize = 50);
	FeatureCollectionManager generatePatchFeatures(QVector<QSharedPointer<TextPatch>> textPatches);

	QVector<QFont> generateFontStyles() const;
	QVector<QFont> generateFonts(int fontCount = 4, QStringList fonts = QStringList(), QVector<QFont> fontStyles = QVector<QFont>()) const;

	void evalSyntheticDataResults(const QVector<QSharedPointer<TextPatch>>& textPatches, 
		const LabelManager labelManager, QString outputDir = QString()) const;

	double computePrecision(const QVector<QSharedPointer<TextPatch>>& textPatches) const;
	void writeEvalResults(QString evalSummary, QString outputDir) const;

	DebugConfig mConfig;
};

class TextHeightEstimationTest {

public:
	TextHeightEstimationTest(const DebugConfig& config = DebugConfig());

	void run();
	void processDirectory(QString dirPath);
	bool drawTextHeightRect(QRect thr);

protected:
	DebugConfig mConfig;
};

class TextBlockFormationTest {

public:
	TextBlockFormationTest(const DebugConfig& config = DebugConfig());

	void run();

protected:
	DebugConfig mConfig;
	void mergeLineRegions(QVector<QSharedPointer<TextLine>>& textLines, Rect mergRect = Rect());
	QSharedPointer<TextRegion> mergeLinesToBlock(QVector<QSharedPointer<TextLine>> textLines, Rect mergRect);
	Polygon computeTextBlockPolygon(QVector<QSharedPointer<TextLine>> textLines);
};

class DebugDavidUtils {

public:
	//static DebugDavidUtils& instance();
	template <typename T>
	static QVector< QSharedPointer<T>> loadRegions(QString imagePath, Region::Type type);

private:
	//DebugDavidUtils();
	//DebugDavidUtils(const DebugDavidUtils&);

};

}
