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

#pragma warning(push, 0)	// no warnings from includes
// Qt Includes
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

		enum ClassifierMode {
			classify__nn = 0,
			classify__nn_wed = 1,
			label_end
		};

		FontStyleClassification();
		FontStyleClassification(const cv::Mat& img, const QVector<QSharedPointer<TextLine>>& textLines);

		bool isEmpty() const override;
		bool compute() override;
		
		static cv::Mat generateTextImage(QString text, QFont font, QRect bbox = QRect(), bool cropImg = false);
		static cv::Mat generateTextPatch(int patchSize, int lineHeight, cv::Mat textImg);
		static cv::Mat generateSyntheticTextPatch(QFont font, QString text);
		static QVector<cv::Mat> generateSyntheticTextPatches(QFont font, QStringList trainingWords);

		QVector<int> classifyTestWords(QVector<cv::Mat> featMatManager, cv::Mat testFeatMat, ClassifierMode mode = ClassifierMode::classify__nn);

		GaborFilterBank createGaborKernels(bool openCV) const;
		QSharedPointer<FontStyleClassificationConfig> config() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		QString toString() const override;

	private:
		// input
		cv::Mat mImg;
		QVector<QSharedPointer<TextLine>> mTextLines;

		// output
		QSharedPointer<ScaleFactory> mScaleFactory;

		bool checkInput() const override;
	};
}
