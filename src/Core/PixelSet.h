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

#pragma once

#include "Shapes.h"
#include "BaseImageElement.h"
#include "Pixel.h"
#include "Algorithms.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QObject>
#include <QSharedPointer>
#include <QVector>
#include <QMap>
#pragma warning(pop)

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

// Qt defines
class QPainter;

namespace rdf {

// elements
class TextLine;
class PixelSet;

/// <summary>
/// Abstract class PixelConnector.
/// This is the base class for all
/// pixel connecting classes which
/// implement different algorithms for
/// connecting super pixels.
/// </summary>
class DllCoreExport PixelConnector {

public:
	PixelConnector();
	
	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const = 0;
	void setDistanceFunction(const PixelDistance::PixelDistanceFunction& distFnc);
	void setStopLines(const QVector<Line>& stopLines);

protected:
	PixelDistance::PixelDistanceFunction mDistanceFnc;
	QVector<Line> mStopLines;

	QVector<QSharedPointer<PixelEdge> > filter(QVector<QSharedPointer<PixelEdge> >& edges) const;
};

/// <summary>
/// Connects pixels using the Delaunay triangulation.
/// </summary>
/// <seealso cref="PixelConnector" />
class DllCoreExport DelaunayPixelConnector : public PixelConnector {

public:
	DelaunayPixelConnector();
	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;
};

/// <summary>
/// Connects pixels using the Voronoi diagram.
/// NOTE: this is highly experimental for it
/// creates new pixels instead of connecting
/// the input pixels (if you need this, take a
/// look at the DelaunayPixelConnector).
/// </summary>
/// <seealso cref="PixelConnector" />
class DllCoreExport VoronoiPixelConnector : public PixelConnector {

public:
	VoronoiPixelConnector();
	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;
};

/// <summary>
/// Fully connected graph.
/// Super pixels are connected with all other super pixels within a region.
/// </summary>
/// <seealso cref="PixelConnector" />
class DllCoreExport RegionPixelConnector : public PixelConnector {

public:
	RegionPixelConnector(double multiplier = 2.0);

	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	void setRadius(double radius);
	void setLineSpacingMultiplier(double multiplier);

protected:
	double mRadius = 0.0;
	double mMultiplier = 2.0;
};

/// <summary>
/// Connects tab stops.
/// </summary>
/// <seealso cref="PixelConnector" />
class DllCoreExport TabStopPixelConnector : public PixelConnector {

public:
	TabStopPixelConnector();

	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

	void setLineSpacingMultiplier(double multiplier);

protected:
	double mMultiplier = 1.0;

};

/// <summary>
/// Connects Pixels using the DBScan.
/// </summary>
/// <seealso cref="PixelConnector" />
class DllCoreExport DBScanPixelConnector : public PixelConnector {

public:
	DBScanPixelConnector();

	virtual QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& pixels) const override;

protected:
	double mEpsMultiplier = 1.2;

};

/// <summary>
/// PixelSet stores and manipulates pixel collections.
/// </summary>
/// <seealso cref="BaseElement" />
class DllCoreExport PixelSet : public BaseElement {

public:
	PixelSet();
	PixelSet(const QVector<QSharedPointer<Pixel> >& set);

	enum ConnectionMode {
		connect_Delaunay,
		connect_region,

		connect_end
	};

	enum mDrawFlags {
		draw_nothing	= 0x0,
		draw_pixels		= 0x1,
		draw_poly		= 0x2,
		draw_rect		= 0x4,

		draw_end
	};
	typedef Flags<mDrawFlags> DrawFlags;

	void operator+=(const PixelSet& set);

	QSharedPointer<Pixel> operator[](int idx) const;
	void operator<<(const QSharedPointer<Pixel>& pixel);
	void operator<<(const PixelSet& set);

	bool isEmpty() const;
	bool contains(const QSharedPointer<Pixel>& pixel) const;
	
	// functions that change the set
	virtual void add(const QSharedPointer<Pixel>& pixel);
	virtual void remove(const QSharedPointer<Pixel>& pixel);
	virtual void append(const QVector<QSharedPointer<Pixel> >& set);
	virtual void scale(double factor);
	virtual void filterDuplicates(int eps = 5);

	QVector<QSharedPointer<Pixel> > pixels() const {
		return mSet;
	};

	int size() const;
	QVector<Vector2D> pointSet(double offsetAngle = 0.0) const;
	QVector<Vector2D> centers() const;
	Polygon convexHull() const;
	Rect boundingBox() const;
	Line fitLine(double offsetAngle = 0.0) const;
	Ellipse fitEllipse() const;

	Vector2D center() const;
	Vector2D meanCenter() const;
	double orientation(double statMoment = 0.5) const;
	double lineSpacing(double statMoment = 0.5) const;
	double area() const;

	QSharedPointer<Pixel> find(const QString& id) const;

	QSharedPointer<TextLine> toTextLine() const;

	virtual void draw(
		QPainter& p, 
		const DrawFlags& options = DrawFlags() | draw_pixels | draw_poly,
		const Pixel::DrawFlags& pixelOptions = Pixel::DrawFlags() | Pixel::draw_ellipse | Pixel::draw_label_colors) const;

	static QVector<QSharedPointer<PixelEdge> > connect(const QVector<QSharedPointer<Pixel> >& superPixels, const ConnectionMode& mode = connect_Delaunay);
	static QVector<PixelSet> fromEdges(const QVector<QSharedPointer<PixelEdge> >& edges);
	static PixelSet merge(const QVector<PixelSet>& sets);
	QVector<PixelSet> splitScales() const;

	virtual QString toString() const override;

protected:
	QVector<QSharedPointer<Pixel> > mSet;

	Polygon polygon(const QVector<Vector2D>& pts) const;
};

class DllCoreExport TextLineSet : public PixelSet {

public:
	TextLineSet();
	TextLineSet(const QVector<QSharedPointer<Pixel> >& set);

	// functions that change the set
	void add(const QSharedPointer<Pixel>& pixel) override;
	void remove(const QSharedPointer<Pixel>& pixel) override;
	void append(const QVector<QSharedPointer<Pixel> >& set) override;
	void scale(double factor) override;
	void update();

	void draw(QPainter& p, const DrawFlags& options = PixelSet::draw_poly, 
		const Pixel::DrawFlags& pixelOptions = Pixel::draw_ellipse) const override;

	Line line() const;
	double error() const;
	double computeError(const QVector<Vector2D>& pts) const;
	double density() const;
	QSharedPointer<Pixel> convertToPixel() const;
	//QSharedPointer<TextRegionPixel> convertToTextRegionPixel() const;

protected:
	Line mLine;
	double mLineErr = DBL_MAX;

	void updateLine();
};

class DllCoreExport WSTextLineSet : public TextLineSet {

public:
	WSTextLineSet();
	WSTextLineSet(const QVector<QSharedPointer<Pixel> >& set);
	WSTextLineSet(const QVector<QSharedPointer<Pixel> >& set, 
		const QVector<QSharedPointer<WhiteSpacePixel> >& wsSet);
	bool containsWhiteSpace(const QSharedPointer<WhiteSpacePixel>& wsPixel) const;
	void setWhiteSpacePixels(const QVector<QSharedPointer<WhiteSpacePixel> >& wsSet);
	void updateMaxGap(const QVector<QSharedPointer<WhiteSpacePixel>>& wsSet =
		QVector<QSharedPointer<WhiteSpacePixel>>());
	void appendWhiteSpaces(const QVector<QSharedPointer<WhiteSpacePixel>>& wsSet);
	void addWhiteSpace(const QSharedPointer<WhiteSpacePixel>& ws);
	void mergeWSTextLineSet(const QSharedPointer<WSTextLineSet>& tls);
	QVector<QSharedPointer<WSTextLineSet>> splitWSTextLineSet(QSharedPointer<WhiteSpacePixel> splitPixel);
	//QSharedPointer<TextRegionPixel> convertToPixel() const;
	QVector<QSharedPointer<WhiteSpacePixel> > whiteSpacePixels() const {
		return mWsSet;
	};
	void setMinBCRSize(double minBCRSize) { mMinBCRSize = minBCRSize; };
	double minBCRSize() { return mMinBCRSize; };
	
	double maxGap() const;
	bool isShort(double minBCRSize = -1) const;
	double pixelHeight(double statMoment = 0.5) const;
	double avgPixelHeight() const;
	double pixelWidth(double statMoment = 0.5) const;

protected:
	QVector<QSharedPointer<WhiteSpacePixel> > mWsSet;
	double mMaxGap = 0;
	double mMinBCRSize = 0;
};

namespace TextLineHelper {

	QVector<QSharedPointer<TextLineSet> > filterLowDensity(const QVector<QSharedPointer<TextLineSet> >& textLines);
	QVector<QSharedPointer<TextLineSet> > filterHeight(const QVector<QSharedPointer<TextLineSet> >& textLines, double minHeight = 5, double maxHeight = 5000);
	QVector<QSharedPointer<TextLineSet> > filterAngle(const QVector<QSharedPointer<TextLineSet> >& textLines, double maxAngle = 4 * DK_DEG2RAD);

	void mergeStableTextLines(QVector<QSharedPointer<TextLineSet> >& textLines);

	QSharedPointer<TextLineSet> find(const QString& id, const QVector<QSharedPointer<TextLineSet> >& tl);
	bool merge(const QSharedPointer<TextLineSet> & tl1, const QSharedPointer<TextLineSet> & tl2);

}

/// <summary>
/// Represents a text block.
/// A single text block has a
/// boundary region and (possibly)
/// a set of super pixels that are
/// geometrically part of the text region.
/// </summary>
/// <seealso cref="BaseElement" />
class DllCoreExport TextBlock : public BaseElement {

public:
	TextBlock(const Polygon& poly = Polygon(), const QString& id = "");

	enum mDrawFlags {
		draw_nothing = 0x0,
		draw_poly = 0x1,
		draw_text_lines = 0x2,
		draw_pixels = 0x4,

		draw_end
	};

	typedef Flags<mDrawFlags> DrawFlags;

	void addPixels(const PixelSet& ps);
	PixelSet pixelSet() const;

	void scale(double factor) override;

	Polygon poly() const;

	void setTextLines(const QVector<QSharedPointer<TextLineSet> >& textLines);
	QVector<QSharedPointer<TextLineSet> > textLines() const;
	bool remove(const QSharedPointer<TextLineSet>& tl);
	void cleanTextLines();

	QSharedPointer<Region> toTextRegion() const;

	void draw(QPainter& p, const DrawFlags& df = DrawFlags() | draw_poly | draw_text_lines);
	virtual QString toString() const override;

private:
	Polygon mPoly;
	PixelSet mSet;
	QVector<QSharedPointer<TextLineSet> > mTextLines;
};

/// <summary>
/// Stores all text blocks.
/// This class is used to group
/// super pixels with respect to
/// layout constrains (e.g. text columns)
/// </summary>
/// <seealso cref="BaseElement" />
class DllCoreExport TextBlockSet : public BaseElement {

public:
	TextBlockSet(const QVector<Polygon>& regions = QVector<Polygon>());
	TextBlockSet(const QVector<QSharedPointer<Region>>& regions);

	void operator<<(const TextBlock& block);

	bool isEmpty() const;

	void scale(double factor) override;

	void setPixels(const PixelSet& ps);
	QVector<QSharedPointer<TextBlock> > textBlocks() const;
	bool remove(const QSharedPointer<TextBlock>& tb);

	QSharedPointer<Region> toTextRegion() const;

	void removeWeakTextLines() const;

private:
	
	QVector<QSharedPointer<TextBlock> > mTextBlocks;
};


/// <summary>
/// Represents a pixel graph.
/// This class comes in handy if you want
/// to map pixel edges with pixels.
/// </summary>
/// <seealso cref="BaseElement" />
class DllCoreExport PixelGraph : public BaseElement {

public:
	PixelGraph();
	PixelGraph(const PixelSet& set);

	enum SortMode {
		sort_none,
		sort_edges,
		sort_line_edges,
		sort_distance,	// euclidean

		sort_end
	};

	bool isEmpty() const;

	void draw(QPainter& p, const PixelDistance::EdgeWeightFunction* weightFnc = 0, double dynamicRange = 1.0) const;
	void connect(const PixelConnector& connector = DelaunayPixelConnector(), const SortMode& sort = sort_none);

	PixelSet set() const;
	QVector<QSharedPointer<PixelEdge> > edges(const QString& pixelID) const;
	QVector<QSharedPointer<PixelEdge> > edges(const QVector<int>& edgeIDs) const;
	QVector<QSharedPointer<PixelEdge> > edges() const;

	int pixelIndex(const QString & pixelID) const;
	QVector<int> edgeIndexes(const QString & pixelID) const;

protected:
	PixelSet mSet;
	QVector<QSharedPointer<PixelEdge> > mEdges;

	QMap<QString, int> mPixelLookup;			// maps pixel IDs to their current vector index
	QMap<QString, QVector<int> > mPixelEdges;	// maps pixel IDs to their corresponding edge index

};

/// <summary>
/// DBScan clustering for pixels.
/// </summary>
class DllCoreExport DBScanPixel {

public:
	DBScanPixel(const PixelSet& pixels);

	void compute();

	void setMaxDistance(double dist);
	void setEpsilonMultiplier(double eps);
	void setDistanceFunction(const PixelDistance::PixelDistanceFunction& distFnc);
	void setFast(bool f);

	QVector<PixelSet> sets() const;
	QVector<QSharedPointer<PixelEdge> > edges() const;

protected:
	PixelSet mPixels;		// input


	enum Label {
		not_visited = 0,
		visited,
		noise, 

		cluster0
	};

	// cache
	cv::Mat mDists;
	cv::Mat mLabels;
	unsigned int* mLabelPtr;

	unsigned int mCLabel = cluster0;

	// parameters
	PixelDistance::PixelDistanceFunction mDistFnc = &PixelDistance::euclidean;
	double mMaxDistance = 0;
	double mEpsMultiplier = 2.0;
	int mMinPts = 3;
	bool mFast = false;

	void expandCluster(int pixelIndex, unsigned int clusterIndex, const QVector<int>& neighbors, double eps, int minPts) const;
	QVector<int> regionQuery(int pixelIdx, double eps) const;

	cv::Mat calcDists(const PixelSet& pixels) const;
};

}
