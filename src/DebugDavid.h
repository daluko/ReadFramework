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

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
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

protected:
	DebugConfig mConfig;
};

class FontStyleClassificationTest {

public:
	FontStyleClassificationTest(const DebugConfig& config = DebugConfig());

	void run();
	void processDirectory(const QString dirPath);
	void testSyntheticDataSet(QString filePath);

protected:
	QStringList loadTextSamples(QString filePath);
	bool generateDataSet(QStringList sample, LabelManager labelManager, QString outputFilePath);
	bool readDataSet(QString inputFilePath, FeatureCollectionManager & fcm, QStringList & samples) const;

	QStringList generateSamplesFromTextFile(QString filePath, int minWordLength = 4, bool removeDuplicates = true);
	QVector<QStringList> splitSampleSet(QStringList samplesSet, double ratio = 0.8);
	
	LabelManager generateFontLabelManager();
	QVector<QSharedPointer<TextPatch>> generateTextPatches(QStringList textSamples, LabelManager labelManager);
	FeatureCollectionManager generatePatchFeatures(QVector<QSharedPointer<TextPatch>> textPatches);
	cv::Mat generateSnytheticTestPage(QString filePath);

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

protected:
	DebugConfig mConfig;
};

}
