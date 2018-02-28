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
#include "SuperPixel.h"
#include "Elements.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QObject>
#include <QSharedPointer>
#include <QDebug>
#include <QColor>
#pragma warning(pop)

#include <opencv2/core.hpp>

#pragma warning (disable: 4251)	// inlined Qt functions in dll interface

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

// Qt defines

namespace rdf {

	// read defines

#define mDebug		qDebug().noquote()		<< debugName()
#define mInfo		qInfo().noquote()		<< debugName()
#define mWarning	qWarning().noquote()	<< debugName()
#define mCritical	qCritical().noquote()	<< debugName()

	class DllCoreExport TextComponent {

	public:
		TextComponent();
		TextComponent(QSharedPointer<MserBlob> blob);

		void setRnn(QSharedPointer<TextComponent> neighbor);
		void setLnn(QSharedPointer<TextComponent> neighbor);
		QSharedPointer<TextComponent> rnn() const;
		QVector<QSharedPointer<TextComponent>> lnn() const;
		QSharedPointer<MserBlob> mserBlob() const;
		void setForkMarker(bool isAFork);
		bool isAFork() const;

		int bestFitLnnIdx() const;
		int lnnRunLength() const;
		bool hasRnn() const;
		bool hasLnn() const;

		void draw(const cv::Mat & img, const QColor& col) const;
		void drawLnns(QPainter& p);

	private:
		QSharedPointer<MserBlob> mMserBlob;
		QSharedPointer<TextComponent> mRnn;
		QVector<QSharedPointer<TextComponent>> mLnn;
		
		bool mHasRnn;
		bool mHasLnn;
		bool mIsAFork;
	};

	class DllCoreExport WhiteSpace {

	public:
		WhiteSpace();
		WhiteSpace(Rect r);
		
		Rect bbox() const;
		bool isNull() const;
		void setIsBCR(bool isBCR);
		void setIsWCC(bool isWCC);
		bool isBCR() const;
		bool isWCC() const;
		bool hasANN() const;
		void setHasANN(bool HasANN);

		QVector<QSharedPointer<WhiteSpace>> bnn();
		void setBnn(const QSharedPointer<WhiteSpace> ws);

	private:
		Rect mBbox;
		bool mIsBCR;
		bool mIsWCC;
		bool mHasANN;
		QVector<QSharedPointer<WhiteSpace>> mBnn;
	};

	class DllCoreExport TextLineCandidate {

	public:
		TextLineCandidate();
		TextLineCandidate(QSharedPointer<TextComponent>);
		//TextLineCandidate(QVector<QSharedPointer<TextComponent>>);

		void merge(QSharedPointer<TextLineCandidate>);
		int length() const;
		Rect bbox() const;

		QVector<QSharedPointer<TextComponent>> textComponents() const;
		QVector<QSharedPointer<WhiteSpace>> whiteSpaces() const;
		void setTextComponents(QVector<QSharedPointer<TextComponent>> textComponents);
		void setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>> whiteSpaces);
		void setMaxGap(double maxGap);
		double maxGap() const;

		void computeWhiteSpaces(int pageWidth);

	private:
		void appendAllRnn(const QSharedPointer<TextComponent> tc);

		QVector<QSharedPointer<TextComponent>> mTextComponents;
		QVector<QSharedPointer<WhiteSpace>> mWhiteSpaces;
		double mMaxGap = -1;
	};

	class DllCoreExport WhiteSpaceRun {
	public:
		WhiteSpaceRun();
		WhiteSpaceRun(QSharedPointer<WhiteSpace> ws);

		void appendAllBnn(const QSharedPointer<WhiteSpace> tc);
		QVector<QSharedPointer<WhiteSpace>> whiteSpaces();

	private:
		QVector<QSharedPointer<WhiteSpace>> mWhiteSpaces;
		
	};

	class DllCoreExport WhiteSpaceAnalysisConfig : public ModuleConfig {

	public:
		WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig();

		virtual QString toString() const override;

		void setShowResults(bool show);
		bool ShowResults() const;

		void setMinRectsPerSpace(int minRects);
		int minRectsPerSpace() const;


	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		bool mShowResults = true;		// if true, shows result regions in the output image
		int mMinRectsPerSpace = 15;		// the minimum number of white rectangles in a row required to build a white space
	};


	class DllCoreExport WhiteSpaceAnalysis : public Module {

	public:

		WhiteSpaceAnalysis(const cv::Mat& img);

		bool isEmpty() const override;
		bool compute() override;
		QSharedPointer<WhiteSpaceAnalysisConfig> config() const;

		cv::Mat mRnnImg;
		QVector<QSharedPointer<TextRegion>> textLines();
		QVector<QSharedPointer<TextRegion>> textBlocks();

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		QString toString() const override;

	private:
		bool checkInput() const override;
		SuperPixel computeSuperPixels(const cv::Mat & img);

		void extractTextComponents();
		void computeInitialTLC();
		void findBCR();
		void splitTLC();
		void groupWS();
		void updateBCR();
		bool updateSegmentation();
		void appendTextLines(int idx, QVector<QVector<int>> bnnIndices, 
			QVector<int> annCount, QSharedPointer<rdf::TextRegion> textRegion);



		// input
		cv::Mat mImg;

		// output
		QVector<QSharedPointer<MserBlob>> mBlobs;
		QVector<QSharedPointer<TextComponent>> mTcM;
		QVector<QSharedPointer<TextLineCandidate>> mTlcM;
		QVector<QSharedPointer<WhiteSpace>> mWsM;
		QVector<QSharedPointer<WhiteSpaceRun>>mWsrM;
		
		QVector<QSharedPointer<TextRegion>>mTextLines;
		QVector<QSharedPointer<TextRegion>>mTextBlocks;
	};
}
