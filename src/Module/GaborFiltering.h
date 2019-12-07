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
 [1] http://www.cvl.tuwien.ac.at/cvl/
 [2] https://transkribus.eu/Transkribus/
 [3] https://github.com/TUWien/
 [4] http://nomacs.org
 *******************************************************************************************************/

#pragma once

#include "Shapes.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QSharedPointer>
#include <QJsonObject>
#include <opencv2/core.hpp>

#pragma warning(pop)

#pragma warning (disable: 4251)	// inlined Qt functions in dll interface

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

namespace rdf {

class DllCoreExport GaborFilterBank {

public:
	GaborFilterBank();
	GaborFilterBank(QVector<double> lambda, QVector<double> theta, int kernelSize);

	void setLambda(QVector<double> lambda);
	void setTheta(QVector<double> theta);
	void setKernels(QVector<cv::Mat> kernels);

	QVector<double> lambda() const;
	QVector<double> theta() const;
	QVector<cv::Mat> kernels() const;
	int kernelSize() const;

	bool isEmpty() const;
	QVector<cv::Mat> draw();
	QString toString();

	void toJson(QJsonObject& jo) const;
	static GaborFilterBank fromJson(QJsonObject& jo);
	static GaborFilterBank read(QString filePath);
	static QString jsonKey();	

private:
	QVector<cv::Mat> mKernels;
	QVector<double> mLambda;
	QVector<double> mTheta;
	int mKernelSize = 128;
};

/// <summary>
/// Contains basic functions to perform gabor filtering on images.
/// </summary>
class DllCoreExport GaborFiltering {

public:
	static cv::Mat createGaborKernel(int ksize, double lambda, double theta, double sigma);
	static QVector<cv::Mat> createGaborKernels(QVector<double> lambda, QVector<double> theta,
		int ksize, double sigma = -1, double psi = 0.0, double gamma = 1.0, bool openCV = true);
	static cv::Mat extractGaborFeatures(cv::Mat img, GaborFilterBank);
};

}