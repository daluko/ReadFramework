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

	class DllCoreExport TextBlockFormationConfig : public ModuleConfig {

	public:
		TextBlockFormationConfig();

		virtual QString toString() const override;

		void setPolygonType(int pt);
		int polygonType() const;

	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		int mPolygonType = 0;
	};


	class DllCoreExport TextBlockFormation : public Module {

	public:

		TextBlockFormation();
		TextBlockFormation(const cv::Mat img, const QVector<QSharedPointer<WSTextLineSet>> textLines);

		enum PolygonType {
			poly_contour = 0,
			poly_convex,
			poly_bb,
			//poly_morph,
			poly_end
		};

		bool isEmpty() const override;
		bool compute() override;

		QSharedPointer<TextBlockFormationConfig> config() const;

		void computeTextBlocks(PixelGraph pg);
		void addSeparatorLines(const QVector<Line>& lines);

		PixelGraph computeTextLineGraph(PixelSet ps);
		TextBlockSet textBlockSet();
		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor());

	protected:
		bool checkInput() const override;

		void computeAdjacency(PixelGraph pg);
		void refineTextBlocks();
		
		TextBlock createTextBlock(const QVector<QSharedPointer<TextLineSet>>& textLines);
		QPolygonF computeTextBlockPoly(const QVector<QSharedPointer<TextLine>>& textLines, int polyType = PolygonType::poly_contour);

		double computeInterLineDistance(const QSharedPointer<WSTextLineSet>& ls1, const QSharedPointer<WSTextLineSet>& ls2);

		//input
		cv::Mat mImg;
		QVector<Line> mSeparatorLines;

		QMap<QString, QSharedPointer<WSTextLineSet>> lineLookUp;
		QMap<QString, int> annCount;
		QMap<QString, QVector<int>> bnnIndices;
		
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
	/// Connects text lines to their nearest neighbors in vertical direction (below only) based on their height.
	/// </summary>
	/// <seealso cref="PixelConnector" />
	class DllCoreExport TLConnector : public PixelConnector {

	public:
		TLConnector();
		TLConnector(cv::Mat img);
		virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	protected:
		cv::Mat mImg;
	};

	class DllCoreExport TextLineHypothesizerConfig : public ModuleConfig {

	public:
		TextLineHypothesizerConfig();

		virtual QString toString() const override;

		void setMinLineLength(int length);
		int minLineLength() const;

		void setErrorMultiplier(double multiplier);
		double errorMultiplier() const;

		QString debugPath() const;

	protected:

		int mMinLineLength = 3;				// minimum text line length when clustering
		double mErrorMultiplier = 1.2;		// maximal increase of error when merging two lines
		QString mDebugPath = "E:/data/test/HBR2013_training";
		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;
	};

	class DllCoreExport TextLineHypothesizer : public Module {
	public:
		TextLineHypothesizer(const cv::Mat img, const PixelSet& set = PixelSet());
		
		bool isEmpty() const override;
		bool compute();
		QSharedPointer<TextLineHypothesizerConfig> config() const;

		QVector<QSharedPointer<TextLine> > textLines() const;
		QVector<QSharedPointer<WSTextLineSet>> textLineSets() const;
		void addSeparatorLines(const QVector<Line>& lines);

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		cv::Mat drawGraphEdges(const cv::Mat & img, const QColor & col = QColor());
		cv::Mat drawTextLineHypotheses(const cv::Mat& img) const;

	protected:
		double minAgreeRatio = 0.5;

		PixelSet mSet;
		cv::Mat mImg;
		QVector<QSharedPointer<WSTextLineSet> > mTextLines;
		QVector<Line> mStopLines;
		PixelGraph mPg;

		bool checkInput() const override;
		QVector<QSharedPointer<WSTextLineSet> > clusterTextLines(const PixelGraph& graph) const;
		int findSetIndex(const QSharedPointer<Pixel>& pixel, const QVector<QSharedPointer<WSTextLineSet> >& sets) const;
		bool mergePixels(const QSharedPointer<PixelEdge> &e) const;
		bool addPixel(QSharedPointer<WSTextLineSet> &set, const QSharedPointer<PixelEdge> &e, const QSharedPointer<Pixel> &pixel, double heat) const;
		bool isContinuousMerge(const QSharedPointer<WSTextLineSet>& tls, const QVector<QSharedPointer<Pixel>>& pixels) const;
		bool mergeTextLines(const QSharedPointer<WSTextLineSet>& tls1, const QSharedPointer<WSTextLineSet>& tls2, const QSharedPointer<PixelEdge> &e, double heat) const;
		bool processEdge(const QSharedPointer<PixelEdge>& edge, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat = -1) const;
		bool processEdgeDebug(const QSharedPointer<PixelEdge>& e, QVector<QSharedPointer<WSTextLineSet>>& textLines, double heat, Rect debugWindow = Rect()) const;
		void mergeUnstableTextLines(QVector<QSharedPointer<WSTextLineSet> >& textLines) const;
		void removeShortTextLines();
		void extractWhiteSpaces(QSharedPointer<WSTextLineSet>& textLine) const;
	};

	class DllCoreExport WhiteSpaceSegmentationConfig : public ModuleConfig {

	public:
		WhiteSpaceSegmentationConfig();

		virtual QString toString() const override;

		void setMinLengthMultiplier(double mlm);
		double minLengthMultiplier() const;

		void setSlicingSizeMultiplier(double ssm);
		double slicingSizeMultiplier() const;

		void setFindWhiteSpaceGaps(bool wsGaps);
		bool findWhiteSpaceGaps() const;

	protected:

		double mMinLengthMultiplier = 2.5 * 3;
		double mSlicingSizeMultiplier = 2.5 * 3;

		bool mFindWhiteSpaceGaps = true;

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;
	};

	class DllCoreExport WhiteSpaceSegmentation : public Module {
	public:
		WhiteSpaceSegmentation();
		WhiteSpaceSegmentation(const cv::Mat img,const QVector<QSharedPointer<WSTextLineSet>>& tlSet);

		bool isEmpty() const override;
		bool compute();
		QSharedPointer<WhiteSpaceSegmentationConfig> config() const;

		void addSeparatorLines(const QVector<Line>& lines);

		QVector<QSharedPointer<WSTextLineSet>> textLineSets() const;
		QVector<QSharedPointer<WhiteSpacePixel>> bcrSet() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawSplitTextLines(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawWhiteSpaceRuns(const cv::Mat& img, const QColor& col = QColor());
		cv::Mat drawWhiteSeparators(const cv::Mat & img, const QColor & col = QColor());
		cv::Mat drawPixelGraph(const cv::Mat& img, const QColor& col = QColor());

	protected:
		//input
		cv::Mat  mImg;
		QVector<QSharedPointer<WSTextLineSet> > mInitialTls;
		QVector<Line> mSeparatorLines;
		QVector<Line> mWhiteSeparatorLines;

		//output
		QVector<QSharedPointer<WSTextLineSet> > mTlsM;
		QVector<QSharedPointer<WhiteSpacePixel>> mBcrM;
		QMap<QString, QVector<QSharedPointer<WSTextLineSet>>> mBcrNeighbors;
		QVector<QSharedPointer<WhiteSpaceRun>> mWsrM;
		PixelGraph mPg;

		bool checkInput() const override;
		bool splitTextLines();
		double computeLineSpacing() const;
		PixelGraph computeSegmentationGraph() const;
		bool removeIsolatedBCR(const PixelGraph pg);
		bool mergeShortTextLines();
		void deleteBCR(const QVector<QSharedPointer<WhiteSpacePixel>>& bcrM);
		void deleteBCR(const QSharedPointer<WhiteSpacePixel>& bcr);
		bool findWhiteSpaceRuns(const PixelGraph pg);
		void updateBCRStatus();
		bool refineWhiteSpaceRuns();
		QVector<Line> computeWhiteSpaceGaps();
		void splitAtWhiteSeparators();
		bool refineTextLineResults();
	};

	class DllCoreExport WhiteSpaceAnalysisConfig : public ModuleConfig {

	public:
		WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig();

		virtual QString toString() const override;

		void setNumErosionLayers(int numErosionLayers);
		int numErosionLayers() const;

		void setMaxImgSide(int maxImgSide);
		int maxImgSide() const;

		void setTextHeightMultiplier(double maxImgSide);
		double textHeightMultiplier() const;

		void setDebugDraw(bool show);
		bool debugDraw() const;

		void setScaleInput(bool scaleInput);
		bool scaleInput() const;

		void setAthenaScaling(bool athenaScaling);
		bool athenaScaling() const;

		void setBlackSeparators(bool bs);
		bool blackSeparators() const;

		void setDebugPath(const QString & dp);
		QString debugPath() const;

	protected:

		void load(const QSettings& settings) override;
		void save(QSettings& settings) const override;

		//MSER & super pixel parameters
		int mNumErosionLayers = 0;
		int mMaxImgSide = 2000;

		//THE and scaling parameters
		double mTextHeightMultiplier = 2.0;
		bool mScaleInput = true;
		bool mAthenaScaling = false;
		bool mBlackSeparators = false;

		bool mDebugDraw = false;
		QString mDebugPath;
	};
	
	class DllCoreExport WhiteSpaceAnalysis : public Module {

	public:

		WhiteSpaceAnalysis(const cv::Mat& img);

		bool isEmpty() const override;
		bool compute() override;

		QSharedPointer<WhiteSpaceAnalysisConfig> config() const;

		void setTlhConfig(QSharedPointer<TextLineHypothesizerConfig> tlhConfig);
		void setWssConfig(QSharedPointer<WhiteSpaceSegmentationConfig> wssConfig);
		void setTbfConfig(QSharedPointer<TextBlockFormationConfig> tbfConfig);
		
		QVector<QSharedPointer<TextRegion>> textLineRegions() const;
		QVector<QSharedPointer<TextLine>> textLineHypotheses() const;
		QSharedPointer<Region> textBlockRegions() const;
		QVector<QSharedPointer<Region>> evalTextBlockRegions() const;

		cv::Mat draw(const cv::Mat& img, const QColor& col = QColor()) const;
		QString toString() const override;

	protected:
		// input
		cv::Mat mImg;
		int mMinPixelsPerBlock;
		int mMinTextHeight = 50;
		QSharedPointer<TextLineHypothesizerConfig> mTlhConfig;
		QSharedPointer<WhiteSpaceSegmentationConfig> mWssConfig;
		QSharedPointer<TextBlockFormationConfig> mTbfConfig;

		//debug output
		Rect filterRect = Rect();
		QVector<QSharedPointer<Pixel>> removedPixels1;
		QVector<QSharedPointer<Pixel>> removedPixels2;

		// output
		PixelSet pSet;
		int mtextHeightEstimate = -1;
		int mtextHeightAthena = -1;
		bool roughEstimateValid = false;
		bool athenaEstimateValid = false;

		QSharedPointer<ScaleFactory> mScaleFactory;
		QVector<Line> mBlackSeparators;
		QVector<QSharedPointer<WSTextLineSet>> mTextLineHypotheses;
		QVector<QSharedPointer<WSTextLineSet>> mWSTextLines;
		QSharedPointer<Region>mTextBlockRegions;
		TextBlockSet mTextBlockSet;

		bool checkInput() const override;
		bool scaleInputImage(double scaleFeactor = 1);
		bool validateImageScale(cv::Mat img);
		void reconfigScaleFactory(int maxImgSide);
		SuperPixel computeSuperPixels(const cv::Mat & img);
		bool computeLocalStats(PixelSet & pixels) const;
		Rect filterPixels(PixelSet& pixels);
		QVector<QVector<QSharedPointer<rdf::Pixel>>> findPixelGroups(PixelSet& set);
		QVector<Line> findBlackSeparators() const;

		//debug
		void drawDebugImages(const cv::Mat& img);
		cv::Mat drawWhiteSpaces(const cv::Mat& img);
		cv::Mat drawFilteredPixels(const cv::Mat& img, QColor fpCol = ColorManager::blue(), 
										QColor bpCol = ColorManager::lightGray(), QColor opCol = ColorManager::red());
		cv::Mat drawBlackSeparators(const cv::Mat & img);
		void drawTheDebugImages(const cv::Mat & img);
	};

	namespace WSAHelper {

		void fftShift(cv::Mat out);

	}
}
