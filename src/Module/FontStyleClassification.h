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

#pragma once

#include "BaseModule.h"
#include "PixelSet.h"
#include "Image.h"
#include "ScaleFactory.h"
#include "GaborFiltering.h"
#include "SuperPixelTrainer.h"
#include "Elements.h"

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
#include <QFont>

//opencv includes
#include <opencv2/ml.hpp>
#pragma warning(pop)

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace rdf {

	class DllCoreExport TextPatch : public BaseElement {

	public:
		TextPatch();
		TextPatch(cv::Mat tpImg, int fixedPatchSize = -1, int textureSize = -1, const QString& id = QString());
		TextPatch(cv::Mat textImg, Rect tpRect, int fixedPatchSize = -1, int textureSize = -1, const QString& id = QString());
		TextPatch(QString text, const LabelInfo label, int fixedPatchSize = -1, int textureSize = -1,
			bool cropPatch = false,  const QString & id = QString());

		bool isEmpty() const;

		int textureSize() const;
		void setTextureSize(int texSize);

		QSharedPointer<PixelLabel> label() const;
		cv::Mat patchTexture() const;
		cv::Mat textPatchImg() const;
		int maxPatchHeight() const;

		void setPolygon(const Polygon& polygon);
		Polygon polygon() const;

	protected:
		int mTextureSize = 128;
		
		double mMinLineNumber = 2.0;
		double mMaxPatchHeight = floor(mTextureSize / mMinLineNumber);

		//input
		cv::Mat mTextPatch;
		QFont mFont;
		Polygon mPoly;

		//io
		QSharedPointer<PixelLabel> mLabel = QSharedPointer<PixelLabel>::create();

		//output
		cv::Mat mPatchTexture;

		bool generatePatchTexture();
		bool generateTextImage(QString text, QFont font, bool cropImg = false);

		void adaptPatchHeight(int patchHeight);
		void adaptPatchHeight(cv::Mat img, Rect tpRect, int patchHeight);
	};

		class DllCoreExport FontDataGenerator{

	public:
		static int computePatchSizeEstimate(QString dataSetDir);
		static int computePatchSizeEstimate(QStringList samples, QVector<QFont> fonts);

		static QVector<QFont> generateFontStyles();
		static QVector<QFont> generateFonts(int fontCount = 4, QStringList fonts = QStringList(), QVector<QFont> fontStyles = QVector<QFont>());
		static LabelManager generateFontLabelManager(QVector<QFont> fonts);

		static QVector<QSharedPointer<TextPatch>> generateDirTextPatches(QString dirPath, int patchHeight = 50, int textureSize = -1, QSharedPointer<LabelManager> lm = QSharedPointer<LabelManager>::create());
		static QVector<QSharedPointer<TextPatch>> generateTextPatches(QString imagePath, int patchHeight = 50, QSharedPointer<LabelManager> lm = QSharedPointer<LabelManager>::create(), int textureSize = -1);
		static QVector<QSharedPointer<TextPatch>> generateTextPatches(QStringList textSamples, LabelManager labelManager, int patchHeight = -1, int textureSize = -1);
		static QVector<QSharedPointer<TextPatch>> generateTextPatches(QVector<QSharedPointer<TextLine>> wordRegions, cv::Mat img, 
			QSharedPointer<LabelManager> manager = QSharedPointer<LabelManager>::create(), int patchHeight = 50, int textureSize = -1);

		static FeatureCollectionManager computePatchFeatures(QVector<QSharedPointer<TextPatch>> textPatches, GaborFilterBank gfb = GaborFilterBank(), bool addLabels = true);

		static bool readDataSet(QString inputFilePath, FeatureCollectionManager & fcm, QStringList & samples);
		static bool generateDataSet(QStringList sample, QVector<QFont> fonts, GaborFilterBank gfb, QString outputFilePath, int patchHeight = -1, int textureSize = -1, bool addLabels = true);
		static bool generateDataSet(QString dataSetPath, GaborFilterBank gfb, int patchSize, int textureSize = -1, QString outputFilePath = QString(), bool addLabels = true);

		template <typename T>
		static QVector< QSharedPointer<T>> loadRegions(QString imagePath, Region::Type type) {
			QString xmlPath = rdf::PageXmlParser::imagePathToXmlPath(imagePath);

			rdf::PageXmlParser parser;
			bool xml_found = parser.read(xmlPath);

			if (!xml_found) {
				qWarning() << "Did not find XML file: " << xmlPath;
				return QVector<QSharedPointer<T>>();
			}

			QSharedPointer<PageElement> xmlPage = parser.page();
			QVector<QSharedPointer<T>> regions = RegionManager::filter<T>(xmlPage->rootRegion(), type);

			return regions;
		}

		static cv::Mat drawTextPatches(QVector<QSharedPointer<TextPatch>> patches, bool drawPatchTexture = false);
	
	private:
		
	};

	class DllCoreExport FontStyleDataSet{

	public:
		FontStyleDataSet();
		FontStyleDataSet(FeatureCollectionManager fcm, int patchHeight, GaborFilterBank gfb);

		bool isEmpty() const;
		int patchHeight() const;
		GaborFilterBank gaborFilterBank() const;
		FeatureCollectionManager featureCollectionManager() const;
		LabelManager labelManager() const;

		void setPatchHeight(int patchHeight);
		void setGFB(GaborFilterBank gfb);
		void setFCM(FeatureCollectionManager & fcm);

		void toJson(QJsonObject & jo, const QString & filePath) const;
		bool write(const QString & filePath) const;

		static FontStyleDataSet fromJson(const QJsonObject & jo, const QString & filePath);
		static FontStyleDataSet read(const QString & filePath);
		static QString jsonKey();

	protected:
		
		int mPatchHeight = -1;
		GaborFilterBank mGFB;
		FeatureCollectionManager mFCM;
		LabelManager mLM;
	};

	class DllCoreExport FontStyleClassifier {

	public:

		FontStyleClassifier(FontStyleDataSet dataSet = FontStyleDataSet(),
			const cv::Ptr<cv::ml::StatModel> model = cv::ml::KNearest::create(), int modelType = FontStyleClassifier::classify_nn_wed);

		enum ClassifierMode {
			classify_nn = 0,
			classify_nn_wed = 1,
			classify_knn = 2,
			classify_bayes = 3,
			classify_svm = 4,
			classify_end
		};

		bool isEmpty() const;
		bool isTrained() const;

		QVector<LabelInfo> classify(cv::Mat testFeat);

		LabelManager manager() const;
		GaborFilterBank gaborFilterBank() const;

		cv::Ptr<cv::ml::StatModel> model() const;
		cv::Ptr<cv::ml::SVM> svm() const;
		cv::Ptr<cv::ml::NormalBayesClassifier> bayes() const;
		cv::Ptr<cv::ml::KNearest> kNearest() const;

		cv::Mat draw(const cv::Mat& img) const;

		bool write(const QString & filePath) const;
		static QSharedPointer<FontStyleClassifier> read(const QString & filePath);

	private:
		cv::Ptr<cv::ml::StatModel> mModel;
		ClassifierMode mClassifierMode;
		FeatureCollectionManager mFCM;
		GaborFilterBank mGFB;
		
		bool checkInput() const;

		void toJson(QJsonObject & jo, QString filePath) const;
		QString jsonKey() const;
		static cv::Ptr<cv::ml::StatModel> readStatModel(QJsonObject & jo, ClassifierMode mode);
	};

	class DllCoreExport FontStyleClassificationConfig : public ModuleConfig {

	public:
		FontStyleClassificationConfig();

		virtual QString toString() const override;

		void setTestBool(bool remove);
		bool testBool() const;

		void setTestInt(int minPx);
		int testInt() const;

		void setTestPath(const QString& cp);
		QString testPath() const;

	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		bool mTestBool = true;
		int mTestInt = 0;
		QString mTestPath = "";
	};

	class DllCoreExport FontStyleClassification : public Module {

	public:
		
		FontStyleClassification();
		FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines);
		FontStyleClassification(const QVector<QSharedPointer<TextPatch>>& patches, QString featureFilePath = QString());

		enum mDrawFlags {
			draw_nothing = 0x0,
			draw_comparison = 0x1,
			draw_gt = 0x2,
			draw_patch_results = 0x3,
			draw_end
		};
		typedef Flags<mDrawFlags> DrawFlags;

		bool isEmpty() const override;
		bool compute() override;

		void setClassifier(const QSharedPointer<FontStyleClassifier>& classifier);
		
		static QFont labelNameToFont(QString labelName);
		static QString fontToLabelName(QFont font);

		//static QVector<cv::Mat> generateSyntheticTextPatches(QFont font, QStringList trainingWords);
		static cv::Mat computeGaborFeatures(QVector<QSharedPointer<TextPatch>> patches, GaborFilterBank gfb, cv::ml::SampleTypes featureType = cv::ml::ROW_SAMPLE);
		static FeatureCollectionManager generateFCM(cv::Mat features);
		static FeatureCollectionManager generateFCM(QVector<QSharedPointer<TextPatch>> patches, cv::Mat features);

		//output
		QVector<QSharedPointer<TextPatch>> textPatches() const;
		bool mapStyleToPatches(QVector<QSharedPointer<TextPatch>>& regionPatches) const;

		cv::Mat labelMap() const;
		cv::Mat draw(const cv::Mat& img) const;
		cv::Mat draw(const cv::Mat& img, QVector<QSharedPointer<TextPatch>> patches,
			const DrawFlags& options = DrawFlags() | draw_gt | draw_comparison | draw_patch_results) const;
		
		QString toString() const override;
		QSharedPointer<FontStyleClassificationConfig> config() const;

	private:
		QSharedPointer<ScaleFactory> mScaleFactory;
		GaborFilterBank mGfb = GaborFilterBank();

		// input
		cv::Mat mImg;
		bool mProcessLines = true;
		QSharedPointer<FontStyleClassifier> mClassifier;
		QString mFeatureFilePath;
		FeatureCollectionManager mFCM_test;
		QVector<QSharedPointer<TextLine>> mTextLines;

		//io
		QVector<QSharedPointer<TextPatch>> mTextPatches;

		bool checkInput() const override;
		QVector<QSharedPointer<TextPatch>> splitTextLine(cv::Mat lineImg, Rect bbox);
		bool processPatches();
		bool loadFeatures();
	};
}
