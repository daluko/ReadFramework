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

#include "Shapes.h"
#include "Pixel.h"
#include "PixelSet.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QRectF>
#include <QVector>
#include <QPainter>
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

// read defines

class DllCoreExport SuperPixelConfig : public ModuleConfig {

public:
	SuperPixelConfig();

	virtual QString toString() const override;

	int mserMinArea() const;
	int mserMaxArea() const;
	int erosionStep() const;
	
	void setNumErosionLayers(int numLayers);
	int numErosionLayers() const;


protected:
	int mMserMinArea = 25;
	int mMserMaxArea = 500;
	int mErosionStep = 4;
	int mNumErosionLayers = 3;

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};

class MserContainer {

public:
	MserContainer() {};

	void append(const MserContainer& o);
	
	QVector<QSharedPointer<MserBlob> > toBlobs() const;
	size_t size() const;

	std::vector<std::vector<cv::Point> > pixels;
	std::vector<cv::Rect> boxes;
};

class DllCoreExport SuperPixel : public Module {

public:
	SuperPixel(const cv::Mat& img);

	bool isEmpty() const override;
	bool compute() override;
		
	QString toString() const override;
	QSharedPointer<SuperPixelConfig> config() const;

	// results - available after compute() is called
	QVector<QSharedPointer<Pixel> > getSuperPixels() const;
	QVector<QSharedPointer<MserBlob> > getMserBlobs() const;
	PixelSet pixelSet() const;

	cv::Mat draw(const cv::Mat& img) const;
	cv::Mat drawMserBlobs(const cv::Mat& img) const;

private:
	cv::Mat mSrcImg;

	// results
	QVector<QSharedPointer<MserBlob> > mBlobs;
	QVector<QSharedPointer<Pixel> > mPixels;
	
	bool checkInput() const override;

	QSharedPointer<MserContainer> getBlobs(const cv::Mat& img, int kernelSize) const;
	QSharedPointer<MserContainer> mser(const cv::Mat& img) const;
	int filterAspectRatio(MserContainer& blobs, double aRatio = 0.1) const;
	int filterDuplicates(MserContainer& blobs, int eps = 5, int upperBound = -1) const;

};

class DllCoreExport ScaleSpaceSPConfig : public ModuleConfig {

public:
	ScaleSpaceSPConfig();

	virtual QString toString() const override;

	int numLayers() const;
	int minLayer() const;

protected:
	int mNumLayers = 3;
	int mMinLayer = 0;

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};

class DllCoreExport ScaleSpaceSuperPixel : public Module {

public:
	ScaleSpaceSuperPixel(const cv::Mat& img);

	bool isEmpty() const override;
	bool compute() override;

	QString toString() const override;
	QSharedPointer<ScaleSpaceSPConfig> config() const;

	// results - available after compute() is called
	PixelSet superPixels() const;

	cv::Mat draw(const cv::Mat& img) const;

protected:
	cv::Mat mSrcImg;

	// results
	PixelSet mSet;

	bool checkInput() const override;
};


class DllCoreExport LocalOrientationConfig : public ModuleConfig {

public:
	LocalOrientationConfig();

	virtual QString toString() const override;

	int maxScale() const;
	int minScale() const;
	Vector2D scaleIvl() const;
	int numOrientations() const;
	int histSize() const;

	// changable parameters
	void setNumOrientations(int numOr);
	void setMaxScale(int maxScale);
	void setMinScale(int minScale);

protected:
	int mMaxScale = 256;	// radius (in px) of the maximum scale
	int mMinScale = 128;	// radius (in px) of the minimum scale
	int mNumOr = 32;		// number of orientation histograms
	int mHistSize = 64;		// size of the orientation histogram

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};

class DllCoreExport LocalOrientation : public Module {

public:
	LocalOrientation(const PixelSet& set = PixelSet());

	bool isEmpty() const override;
	bool compute() override;

	QString toString() const override;
	QSharedPointer<LocalOrientationConfig> config() const;

	// results - available after compute() is called
	PixelSet set() const;

	cv::Mat draw(const cv::Mat& img, const QString& id, double radius) const;

private:
	
	// input/output
	PixelSet mSet;

	bool checkInput() const override;

	void computeScales(Pixel* pixel, const QVector<Pixel*>& set) const;
	void computeAllOrHists(Pixel* pixel, const QVector<Pixel*>& set, double radius) const;
	void computeOrHist(const Pixel* pixel, 
		const QVector<const Pixel*>& set, 
		const Vector2D& histVec, 
		cv::Mat& orHist,
		float& sparsity) const;

};

};