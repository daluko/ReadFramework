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

	class DllCoreExport TextHeightEstimationConfig : public ModuleConfig {

	public:
		TextHeightEstimationConfig();

		virtual QString toString() const override;

		void setDebugDraw(bool remove);
		bool debugDraw() const;

		void setNumSplitLevels(int nsl);
		int numSplitLevels() const;

		void setDebugPath(const QString& cp);
		QString debugPath() const;

	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		bool mDebugDraw = false;
		int mNumSplitLevels = 5;
		QString mDebugPath = "";
	};

	class DllCoreExport TextHeightEstimation : public Module {

	public:
		TextHeightEstimation(const cv::Mat& img);

		bool isEmpty() const override;
		bool compute() override;
		int textHeightEstimate();
		double coverage();
		double relCoverage();
		double confidence();
		QSharedPointer<TextHeightEstimationConfig> config() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		void drawDebugImages(QString input_path) const;
		QString toString() const override;

	private:
		struct ImagePatch {
			cv::Range xRange;
			cv::Range yRange;
		};

		bool checkInput() const override;
		void subdivideImage(const cv::Mat img, int numSplitLevels);
		QVector<cv::Range> splitRange(const cv::Range range) const;
		cv::Mat processImagePatches(cv::Mat img);
		cv::Mat processImagePatchesDebug(cv::Mat img);
		cv::Mat nacf(const cv::Mat img);
		cv::Mat computePMF(int tMax, double w, double m, double sig);
		void fftShift(cv::Mat _out);
		void computeConfidence(cv::Mat accPMF);

		// input
		cv::Mat mImg;

		// output
		QVector<QVector<QSharedPointer<ImagePatch>>> imagePatches;
		int thEstimate = -1;
		double theConfidence = 0;
		double meanCoverage = 0;
		double meanRelativeCoverage = 0;

		//debug output
		QVector<cv::Mat> nacImages;
		QVector<cv::Mat> magImages;
		cv::Mat accPMFImg;
	};


}
