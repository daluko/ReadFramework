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
		TextPatch(cv::Mat textImg, const QString& id = QString());
		TextPatch(QString text, const LabelInfo label, const QString & id = QString());

		bool isEmpty() const;
		int textureSize() const;
		int textureLineHeight() const;

		void setTextureSize(int numLayers);
		void setTextureLineHeight(int minArea);

		QSharedPointer<PixelLabel> label() const;
		cv::Mat patchTexture() const;
		cv::Mat textPatchImg() const;

	protected:
		//input
		cv::Mat mTextPatch;
		QFont mFont;

		int mTextureSize = 128;
		int mTextureLineHeight = 32;

		//io
		QSharedPointer<PixelLabel> mLabel = QSharedPointer<PixelLabel>::create();

		//output
		cv::Mat mPatchTexture;

		bool generatePatchTexture();
		bool generateTextImage(QString text, QFont font, bool cropImg = false);
	};

	class DllCoreExport FontStyleClassifier{

	public:
		enum ClassifierMode {
			classify_nn = 0,
			classify_nn_wed = 1,
			classify_knn = 2,
			classify_bayes = 3,
			classify_svm = 4,
			classify_end
		};

		FontStyleClassifier(const FeatureCollectionManager& fcm = FeatureCollectionManager(), 
			const cv::Ptr<cv::ml::StatModel>& model = cv::Ptr<cv::ml::StatModel>(), int modelType = FontStyleClassifier::classify_nn_wed);

		bool isEmpty() const;
		cv::Ptr<cv::ml::StatModel> model() const;
		LabelManager manager() const;
		QVector<LabelInfo> classify(cv::Mat testFeat);

		cv::Ptr<cv::ml::SVM> svm() const;
		cv::Ptr<cv::ml::NormalBayesClassifier> bayes() const;
		cv::Ptr<cv::ml::KNearest> kNearest() const;

		//QSharedPointer<FontStyleClassifierConfig> config() const;
		cv::Mat draw(const cv::Mat& img) const;

		bool write(const QString & filePath) const;
		static QSharedPointer<FontStyleClassifier> read(const QString & filePath);

	private:
		cv::Ptr<cv::ml::StatModel> mModel;
		FeatureCollectionManager mFcm;
		ClassifierMode mClassifierMode;

		bool checkInput() const;

		void toJson(QJsonObject & jo) const;
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
		int mTestInt = 15;
		QString mTestPath = "";
	};

	class DllCoreExport FontStyleClassification : public Module {

	public:

		FontStyleClassification();
		FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines);
		FontStyleClassification(const QVector<QSharedPointer<TextPatch>>& textPatches, QString featureFilePath = QString());

		bool isEmpty() const override;
		bool compute() override;

		void setClassifier(const QSharedPointer<FontStyleClassifier>& classifier);
		
		static cv::Mat generateTextImage(QString text, QFont font, QRect bbox = QRect(), bool cropImg = false);
		static cv::Mat generateTextPatch(int patchSize, int lineHeight, cv::Mat textImg);
		static cv::Mat generateSyntheticTextPatch(QFont font, QString text);
		
		static QFont labelNameToFont(QString labelName);
		static QString fontToLabelName(QFont font);

		static QVector<cv::Mat> generateSyntheticTextPatches(QFont font, QStringList trainingWords);
		static GaborFilterBank createGaborKernels(QVector<double> theta = QVector<double>(), QVector<double> lambda = QVector<double>(), bool openCV = true);
		static cv::Mat computeGaborFeatures(QVector<QSharedPointer<TextPatch>> patches, GaborFilterBank gfb, cv::ml::SampleTypes featureType = cv::ml::ROW_SAMPLE);
		static FeatureCollectionManager generateFCM(QVector<QSharedPointer<TextPatch>> patches, cv::Mat features);

		//output
		QVector<QSharedPointer<TextPatch>> textPatches() const;

		QSharedPointer<FontStyleClassificationConfig> config() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		QString toString() const override;

	private:
		// input
		cv::Mat mImg;
		QSharedPointer<FontStyleClassifier> mClassifier;
		bool mProcessLines = true;


		QString mFeatureFilePath;
		FeatureCollectionManager mFCM_test;

		//io
		QVector<QSharedPointer<TextLine>> mTextLines;
		QVector<QSharedPointer<TextPatch>> mTextPatches;

		// output
		QSharedPointer<ScaleFactory> mScaleFactory;

		bool checkInput() const override;
	};

}
