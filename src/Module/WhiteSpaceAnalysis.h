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

	class DllCoreExport WhiteSpaceRun : public BaseElement {
	public:
		WhiteSpaceRun();
		WhiteSpaceRun(QVector<QSharedPointer<WhiteSpacePixel>> wsSet);

		QVector<QSharedPointer<WhiteSpacePixel>> whiteSpaces();
		void append(const QVector<QSharedPointer<WhiteSpacePixel>>& wsSet);
		void add(const QSharedPointer<WhiteSpacePixel>& ws);
		bool contains(const QSharedPointer<WhiteSpacePixel>& ws) const;
		void remove(const QSharedPointer<WhiteSpacePixel>& ws);
		int size() const;
		Rect boundingBox() const;

	protected:
		QVector<QSharedPointer<WhiteSpacePixel>> mWhiteSpaces;

	};

	class DllCoreExport TextBlockFormation {
	public:
		TextBlockFormation();
		TextBlockFormation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>> textLines);

		bool compute();
		TextBlockSet textBlockSet();
		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor());

	protected:
		void computeAdjacency();
		void formTextBlocks();
		void appendTextLines(int idx, QVector<QSharedPointer<TextLineSet> >& textLines);
		TextBlock createTextBlock(const QVector<QSharedPointer<TextLineSet> >& textLines);

		QVector<QVector<int>> bnnIndices;
		QVector<int> annCount;

		cv::Mat mImg;
		QVector<QSharedPointer<WSTextLineSet>>mTextLines;
		TextBlockSet mTextBlockSet;
	};

	/// <summary>
	/// Connects pixels in horizontal direction if they have overlapping y coordinates.
	/// </summary>
	/// <seealso cref="PixelConnector" />
	class DllCoreExport RightNNConnector : public PixelConnector {

	public:
		RightNNConnector();
		virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	protected:
	};

	/// <summary>
	/// Connects white spaces to their nearest neighbors in vertical direction (above + below).
	/// </summary>
	/// <seealso cref="PixelConnector" />
	class DllCoreExport WSConnector : public PixelConnector {

	public:
		WSConnector();
		void setLineSpacing(double lineSpacing);
		virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	protected:
		double mLineSpacing = 0;
	};

	/// <summary>
	/// Connects pixels to their nearest neighbor in horizontal and vertical direction.
	/// </summary>
	/// <seealso cref="PixelConnector" />
	class DllCoreExport NNConnector : public PixelConnector {

	public:
		NNConnector();
		virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	protected:
	};

	class DllCoreExport TextLineHypothisizerConfig : public ModuleConfig {

	public:
		TextLineHypothisizerConfig();

		virtual QString toString() const override;

		void setMinLineLength(int length);
		int minLineLength() const;

		void setErrorMultiplier(double multiplier);
		double errorMultiplier() const;

		QString debugPath() const;

	protected:

		int mMinLineLength = 3;			// minimum text line length when clustering
		double mErrorMultiplier = 1.2;		// maximal increase of error when merging two lines
		QString mDebugPath = "E:/data/test/HBR2013_training";
		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;
	};

	class DllCoreExport TextLineHypothisizer : public Module {
	public:
		TextLineHypothisizer(const cv::Mat img, const PixelSet& set = PixelSet());
		
		bool isEmpty() const override;
		bool compute();
		QSharedPointer<TextLineHypothisizerConfig> config() const;

		QVector<QSharedPointer<TextLine> > textLines() const;
		QVector<QSharedPointer<WSTextLineSet>> textLineSets() const;
		void addSeparatorLines(const QVector<Line>& lines);

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawGraphEdges(const cv::Mat & img, const QColor & col = QColor());
		cv::Mat drawTextLineHypotheses(const cv::Mat& img);

	protected:
		PixelSet mSet;
		cv::Mat mImg;
		QVector<QSharedPointer<WSTextLineSet> > mTextLines;
		QVector<Line> mStopLines;
		PixelGraph mPg;

		bool checkInput() const override;
		QVector<QSharedPointer<WSTextLineSet> > clusterTextLines(const PixelGraph& graph) const;
		int findSetIndex(const QSharedPointer<Pixel>& pixel, const QVector<QSharedPointer<WSTextLineSet> >& sets) const;
		bool addPixel(QSharedPointer<WSTextLineSet>& set, const QSharedPointer<Pixel>& pixel) const;
		bool mergeTextLines(const QSharedPointer<WSTextLineSet>& tls1, const QSharedPointer<WSTextLineSet>& tls2) const;
		bool processEdge(const QSharedPointer<PixelEdge>& edge, QVector<QSharedPointer<WSTextLineSet>>& textLines) const;
		void mergeUnstableTextLines(QVector<QSharedPointer<WSTextLineSet> >& textLines) const;
		void extractWhiteSpaces(QSharedPointer<WSTextLineSet>& textLine) const;
	};

	class DllCoreExport WhiteSpaceSegmentation : public Module {
	public:
		WhiteSpaceSegmentation();
		WhiteSpaceSegmentation(const cv::Mat img,const QVector<QSharedPointer<WSTextLineSet>>& tlSet);

		bool isEmpty() const override;
		bool compute();

		QVector<QSharedPointer<WSTextLineSet>> textLineSets() const;
		QVector<QSharedPointer<WhiteSpacePixel>> bcrSet() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawSplitTextLines(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawWhiteSpaceRuns(const cv::Mat& img, const QColor& col = QColor());

	protected:
		cv::Mat  mImg;
		QVector<QSharedPointer<WSTextLineSet> > mTlsM;
		QVector<QSharedPointer<WhiteSpacePixel>> mBcrM;
		QMap<QString, QVector<QSharedPointer<WSTextLineSet>>> mBcrNeighbors;
		QVector<QSharedPointer<WhiteSpaceRun>> mWsrM;

		bool checkInput() const override;
		bool splitTextLines();
		double computeLineSpacing() const;
		PixelGraph computeSegmentationGraph() const;
		void removeIsolatedBCR(const PixelGraph pg);
		void deleteBCR(const QVector<QSharedPointer<WhiteSpacePixel>>& bcrM);
		void deleteBCR(const QSharedPointer<WhiteSpacePixel>& bcr);
		bool findWhiteSpaceRuns(const PixelGraph pg);
		void updateBCRStatus();
		bool refineWhiteSpaceRuns();
	};

	class DllCoreExport WhiteSpaceAnalysisConfig : public ModuleConfig {

	public:
		WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig();

		virtual QString toString() const override;

		void setNumErosionLayers(int numErosionLayers);
		int numErosionLayers() const;

		void setMserMinArea(int mserMinArea);
		int mserMinArea() const;

		void setMserMaxArea(int mserMaxArea);
		int mserMaxArea() const;

		void setMaxImgSide(int maxImgSide);
		int maxImgSide() const;

		void setDebugDraw(bool show);
		bool debugDraw() const;

		void setScaleInput(bool show);
		bool scaleInput() const;

		void setDebugPath(const QString & dp);
		QString debugPath() const;

	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		//MSER/super pixel parameters
		int mNumErosionLayers = 0; //must be > 0
		int mMserMinArea = 25;
		int mMserMaxArea = 700;
		int mMaxImgSide = 1500;

		bool mScaleInput = true;

		bool mDebugDraw = false;
		QString mDebugPath = "E:/data/test/HBR2013_training";
	};
	
	class DllCoreExport WhiteSpaceAnalysis : public Module {

	public:

		WhiteSpaceAnalysis(const cv::Mat& img);

		bool isEmpty() const override;
		bool compute() override;
		QSharedPointer<WhiteSpaceAnalysisConfig> config() const;

		QVector<QSharedPointer<TextRegion>> textLineRegions() const;
		QSharedPointer<Region> textBlockRegions() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		QString toString() const override;

	protected:
		bool checkInput() const override;
		SuperPixel computeSuperPixels(const cv::Mat & img);
		bool computeLocalStats(PixelSet & pixels) const;
		Rect filterPixels(PixelSet& pixels);

		void drawDebugImages(const cv::Mat& img);
		cv::Mat drawWhiteSpaces(const cv::Mat& img);
		cv::Mat drawFilteredPixels(const cv::Mat& img);

		// input
		cv::Mat mImg;
		int mMinPixelsPerBlock;

		//debug
		Rect filterRect = Rect();
		QVector<QSharedPointer<Pixel>> removedPixels1;
		QVector<QSharedPointer<Pixel>> removedPixels2;

		// output
		PixelSet pSet;
		QVector<QSharedPointer<WSTextLineSet>> mTextLineHypotheses;
		QVector<QSharedPointer<WSTextLineSet>> mWSTextLines;
		QSharedPointer<Region>mTextBlockRegions;
		TextBlockSet mTextBlockSet;
	};
}
