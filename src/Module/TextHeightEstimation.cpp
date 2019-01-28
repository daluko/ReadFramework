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

#include "TextHeightEstimation.h"

#include "Image.h"
#include "Utils.h"
#include "Elements.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QFileInfo>
#include <QDir>
#include <QPainter>
#include "opencv2/imgproc.hpp"
#pragma warning(pop)

namespace rdf {


	// LayoutAnalysisConfig --------------------------------------------------------------------
	TextHeightEstimationConfig::TextHeightEstimationConfig() : ModuleConfig("Text Height Estimation Module") {
	}

	QString TextHeightEstimationConfig::toString() const {
		return ModuleConfig::toString();
	}

	void TextHeightEstimationConfig::setDebugDraw(bool dd) {
		mDebugDraw = dd;
	}

	bool TextHeightEstimationConfig::debugDraw() const {
		return mDebugDraw;
	}

	void TextHeightEstimationConfig::setNumSplitLevels(int nsl) {
		mNumSplitLevels = nsl;
	}

	int TextHeightEstimationConfig::numSplitLevels() const {
		return ModuleConfig::checkParam(mNumSplitLevels, 2, INT_MAX, "numSplitLevels");
	}

	void TextHeightEstimationConfig::setDebugPath(const QString & dp) {
		mDebugPath = dp;
	}

	QString TextHeightEstimationConfig::debugPath() const {
		return mDebugPath;
	}

	void TextHeightEstimationConfig::load(const QSettings & settings) {

		mDebugDraw = settings.value("debugDraw", debugDraw()).toBool();
		mNumSplitLevels = settings.value("numSplitLevels", numSplitLevels()).toInt();
		mDebugPath = settings.value("classifierPath", debugPath()).toString();
	}

	void TextHeightEstimationConfig::save(QSettings & settings) const {

		settings.setValue("debugDraw", debugDraw());
		settings.setValue("numSplitLevels", numSplitLevels());
		settings.setValue("debugDraw", debugPath());
	}

	// TextHeightEstimation --------------------------------------------------------------------
	TextHeightEstimation::TextHeightEstimation(const cv::Mat& img) {

		mImg = img;

		mConfig = QSharedPointer<TextHeightEstimationConfig>::create();
		mConfig->loadSettings();
	}

	bool TextHeightEstimation::isEmpty() const {
		return mImg.empty();
	}

	bool TextHeightEstimation::compute() {

		//TODO fix parameters
		//TODO check weights of PMFs (lower level PMFs seem to have large influence -> results of higher levels will be ignored)
		//TODO check for run time problems (big difference on similar images)

		if (!checkInput())
			return false;

		qInfo() << "Computing text height estimation...";

		Timer dt;

		int numSplitLevels = config()->numSplitLevels();
		imagePatches = QVector<QVector<QSharedPointer<ImagePatch>>>(numSplitLevels);
		cv::Mat img_gray;

		//convert input image to gray scale if needed
		if (mImg.channels() == 1)
			img_gray = mImg;
		else if(mImg.channels() == 3 || mImg.channels() == 4)
			cv::cvtColor(mImg, img_gray, cv::COLOR_BGR2GRAY);
		else
			return false;

		//saves split image patches of input image in imagePatches variable
		subdivideImage(img_gray, numSplitLevels);
		
		//process text patches and get the probability mass function (PMF) of the text height random variable
		cv::Mat accPMF;
		if(!config()->debugDraw())
			accPMF = processImagePatches(img_gray);
		else
			accPMF = processImagePatchesDebug(img_gray);

		//find final estimate for text height (max of PMF)
		cv::Point maxLoc;
		double maxVal;
		cv::minMaxLoc(accPMF, NULL, &maxVal, NULL, &maxLoc);
		thEstimate = maxLoc.x;

		//get estimate for confidence of THE result
		computeConfidence(accPMF);

		qInfo() << "Estimated text height = " << QString::number(thEstimate);
		qInfo() << "Finished text height estimation. Computation took: " << dt.getTotal();

		return true;
	}

	void TextHeightEstimation::subdivideImage(const cv::Mat img, int numSplitLevels) {

		//level 0 won't be used (contribuition very poor according to Pintus et al.)
		QSharedPointer<ImagePatch> p0 = QSharedPointer<ImagePatch>(new ImagePatch{ cv::Range(0, img.cols), cv::Range(0, img.rows) });
		imagePatches[0] << QVector<QSharedPointer<ImagePatch>>(1, p0);

		for (int i = 1; i < numSplitLevels; i++) {
			for (auto p : imagePatches[i - 1]) {
				auto xRs = splitRange(p->xRange);
				auto yRs = splitRange(p->yRange);
				QSharedPointer<ImagePatch> pi_0 = QSharedPointer<ImagePatch>(new ImagePatch{ xRs[0], yRs[0] });
				QSharedPointer<ImagePatch> pi_1 = QSharedPointer<ImagePatch>(new ImagePatch{ xRs[1], yRs[0] });
				QSharedPointer<ImagePatch> pi_2 = QSharedPointer<ImagePatch>(new ImagePatch{ xRs[0], yRs[1] });
				QSharedPointer<ImagePatch> pi_3 = QSharedPointer<ImagePatch>(new ImagePatch{ xRs[1], yRs[1] });

				if (imagePatches[i].isEmpty()) {
					QVector<QSharedPointer<ImagePatch>> pi;
					pi << pi_0 << pi_1 << pi_2 << pi_3;
					imagePatches[i].append(pi);
				}
				else {
					imagePatches[i] << pi_0 << pi_1 << pi_2 << pi_3;
				}
			}
		}
	}

	QVector<cv::Range> TextHeightEstimation::splitRange(cv::Range range) const {

		QVector<cv::Range> splitRanges;

		if ((range.size() % 2) == 0) {
			int rm = range.start + (int)(range.size() / 2);
			cv::Range r1(range.start, +rm);
			cv::Range r2(rm, range.end);

			splitRanges << r1 << r2;
		}
		else {
			int rm = range.start + std::ceil(range.size() / 2.0);
			cv::Range r1(range.start, rm);
			cv::Range r2(rm - 1, range.end);

			splitRanges << r1 << r2;
		}

		return splitRanges;
	}

	cv::Mat TextHeightEstimation::processImagePatches(cv::Mat img){

		cv::Mat accPMF = cv::Mat::zeros(1, ceil(img.rows / 2.0), CV_32F);

		//process each layer
		for (int i = 1; i < imagePatches.size(); ++i) {	//discard level 0 -> contribution is very poor according to Pintus et al.

			auto layer = imagePatches[i];
			cv::Mat maxMagHist = cv::Mat::zeros(100, 1, CV_32F);
			cv::Mat maxMagCount = cv::Mat::zeros(100, 1, CV_8U);
			
			//process each patche of current layer
			for (auto pi : layer) {

				cv::Mat patch = img(pi->yRange, pi->xRange).clone();
				cv::Mat nacImg = nacf(patch); //compute normalize auto correlation
				cv::Mat vPP, vPPdft, mag;

				//compute vertical projection profile of nac image
				reduce(nacImg, vPP, 1, CV_REDUCE_SUM, CV_32F);

				//compute DFT of PP and magnitudes of the DFT coefficients
				cv::dft(vPP, vPPdft, cv::DFT_COMPLEX_OUTPUT);

				std::vector<cv::Mat> planes;
				split(vPPdft, planes);
				magnitude(planes[0], planes[1], mag);

				//find index of coefficient with max magnitude
				mag.at<float>(0, 0) = 0;	//discard contant compoent (0 index coefficient)
				mag = mag(cv::Range(0, std::min(mag.rows / 2, 100)), cv::Range(0, 1));	//ignore coefficients with index > 100 (assumption -> max 100 lines per page)

				double maxVal;
				cv::Point maxLoc;
				cv::minMaxLoc(mag, NULL, &maxVal, NULL, &maxLoc);

				//save max magnitude to magnitude histogram of this patch layer
				if (maxLoc.y > 1) {	//outlier pruning: index <= 1 likely to be figure or sparse patch
					maxMagHist.at<float>(maxLoc) += (float)maxVal;
					maxMagCount.at<uchar>(maxLoc) += 1;
				}
			}

			if (cv::sum(maxMagCount)[0] == 0) {
				continue;	//Layer does not contain relevant coefficients
			}

			//find index of max magnitude of this layer
			cv::Point maxLoc;
			cv::minMaxLoc(maxMagCount, NULL, NULL, NULL, &maxLoc);
			int thIdx = maxLoc.y;

			//compute probability mass function of text height random variable and accumulate it for all levels
			double patchHeight = layer[0]->yRange.size();
			double patchWidth = layer[0]->xRange.size();

			double An = maxMagHist.at<float>(thIdx);
			double Cn = maxMagCount.at<uchar>(thIdx);

			double thMin = patchHeight / ((double)thIdx + 0.5);
			double thMax = patchHeight / ((double)thIdx - 0.5);

			double m = (thMin + thMax) / 2;
			double sig = pow(thMax - m, 2);
			double w = (1 / (patchWidth*Cn)) * An;

			int tMax = accPMF.cols;

			accPMF += computePMF(tMax, w, m, sig);

			double coverage = cv::sum(maxMagCount)[0] / (double)layer.size();
			meanCoverage += coverage / (imagePatches.size() - 1);

			double relativeCoverage = Cn / cv::sum(maxMagCount)[0];
			meanRelativeCoverage += relativeCoverage / (imagePatches.size() - 1);
		}

		return accPMF;
	}

	cv::Mat TextHeightEstimation::processImagePatchesDebug(cv::Mat img){

		cv::Mat accPMF = cv::Mat::zeros(1, ceil(img.rows / 2.0), CV_32F);

		for (int i = 1; i < imagePatches.size(); ++i) {	//discard level 0 -> contribution is very poor according to Pintus et al.

			auto layer = imagePatches[i];
			cv::Mat maxMagHist = cv::Mat::zeros(100, 1, CV_32F);
			cv::Mat maxMagCount = cv::Mat::zeros(100, 1, CV_8U);

			//debug output
			cv::Mat nacLayer = img.clone();

			for (auto pi : layer) {

				cv::Mat patch = img(pi->yRange, pi->xRange).clone();
				cv::Mat nacImg = nacf(patch); //compute normalize auto correlation
				cv::Mat vPP, vPPdft, mag;

				//compute vertical projection profile of nac image
				reduce(nacImg, vPP, 1, CV_REDUCE_SUM, CV_32F);

				//compute DFT of PP and magnitudes of the DFT coefficients
				cv::dft(vPP, vPPdft, cv::DFT_COMPLEX_OUTPUT);

				std::vector<cv::Mat> planes;
				split(vPPdft, planes);
				magnitude(planes[0], planes[1], mag);

				//find index of coefficient with max magnitude
				mag.at<float>(0, 0) = 0;	//discard contant compoent (0 index coefficient)
				mag = mag(cv::Range(0, std::min(mag.rows / 2, 100)), cv::Range(0, 1));	//ignore coefficients with index > 100 (assumption -> max 100 lines per page)

				double maxVal;
				cv::Point maxLoc;
				cv::minMaxLoc(mag, NULL, &maxVal, NULL, &maxLoc);

				//save max magnitude to magnitude histogram of this patch layer
				if (maxLoc.y > 1) {	//outlier pruning: index <= 1 likely to be figure or sparse patch
					maxMagHist.at<float>(maxLoc) += (float)maxVal;
					maxMagCount.at<uchar>(maxLoc) += 1;
				}

				//debug draw-------------------------------------------------------------------------------------------------------------------------
				//draw vertical projection profile within dft image
				nacImg.convertTo(nacImg, CV_8U, 255);

				int max_x = nacImg.cols - 1;
				for (int j = 1; j < vPP.rows; j++) {
					cv::Point p1 = cv::Point(max_x - vPP.at<float>(j - 1), j - 1);
					cv::Point p2 = cv::Point(max_x - vPP.at<float>(j), j);
					cv::line(nacImg, p1, p2, cv::Scalar(255), 2, 8, 0);
				}

				//draw magnitudes
				normalize(mag, mag, 0, nacImg.cols/2.0, CV_MINMAX);
				for (int j = 0; j < mag.rows; j++) {
					cv::Point p1 = cv::Point(0, j);
					cv::Point p2 = cv::Point(mag.at<float>(j), j);
					cv::line(nacImg, p1, p2, cv::Scalar(255), 1, 8, 0);
				}

				//draw patch and result values in original image
				QImage qImg = Image::mat2QImage(nacImg, true);
				QPainter painter(&qImg);
				painter.setPen(QColor(150,150,150,255));
				QString outText = "maxMagIndex=" + QString::number(maxLoc.y) + "\n" + "maxMag=" + QString::number(maxVal);
				painter.drawText(qImg.rect(), QTextOption::WrapAtWordBoundaryOrAnywhere || Qt::AlignTop || Qt::AlignLeft, outText);
				nacImg = Image::qImage2Mat(qImg);
				cv::cvtColor(nacImg, nacImg, cv::COLOR_BGR2GRAY);
				nacImg.copyTo(nacLayer(pi->yRange, pi->xRange));
				//debug draw-------------------------------------------------------------------------------------------------------------------------
			}

			if (cv::sum(maxMagCount)[0] == 0) {
				qDebug() << "Layer does not contain relevant coefficients";
				continue;
			}

			//find index max magnitude
			cv::Point maxLoc;
			cv::minMaxLoc(maxMagCount, NULL, NULL, NULL, &maxLoc);
			int thIdx = maxLoc.y;

			//compute probability mass function of text height random variable and accumulate it for all levels
			double patchHeight = layer[0]->yRange.size();
			double patchWidth = layer[0]->xRange.size();

			double An = maxMagHist.at<float>(thIdx);
			double Cn = maxMagCount.at<uchar>(thIdx);

			double thMin = patchHeight / ((double)thIdx + 0.5);
			double thMax = patchHeight / ((double)thIdx - 0.5);

			double m = (thMin + thMax) / 2;
			double sig = pow(thMax - m, 2);
			double w = (1 / (patchWidth*Cn)) * An;

			int tMax = accPMF.cols;

			accPMF += computePMF(tMax, w, m, sig);

			double coverage = cv::sum(maxMagCount)[0] / (double)layer.size();
			meanCoverage += coverage / (imagePatches.size() - 1);

			double relativeCoverage = Cn / cv::sum(maxMagCount)[0];
			meanRelativeCoverage += relativeCoverage / (imagePatches.size() - 1);

			//debug draw-------------------------------------------------------------------------------------------------------------------------
			//draw the magnitude histogram of the current layer
			cv::Mat normMMH;
			cv::Mat magImg = cv::Mat::zeros(255, maxMagHist.rows, CV_8U);
			normalize(maxMagHist, normMMH, 0, 255, CV_MINMAX);
			normMMH.convertTo(normMMH, CV_8U);

			for (int j = 0; j < maxMagHist.rows; j++) {
				cv::Point p1 = cv::Point(j, magImg.rows-1);
				cv::Point p2 = cv::Point(j, (magImg.rows - 1)- normMMH.at<uchar>(j));
				cv::line(magImg, p1, p2, cv::Scalar(255), 1, 8, 0);

				p1 = cv::Point(j, magImg.rows - 1);
				p2 = cv::Point(j, (magImg.rows - 1) - maxMagCount.at<uchar>(j));
				cv::line(magImg, p1, p2, cv::Scalar(130), 1, 8, 0);
			}

			//print magnitude results to debug image
			QImage qImg = Image::mat2QImage(magImg, true);
			QPainter painter(&qImg);
			painter.setPen(QColor(130, 130, 130, 255));
			QString outText = "maxMagIdx=" + QString::number(thIdx) + "\n" + "maxMag=" + QString::number(An) + "\n" + "maxMagCount=" + QString::number(Cn) + "\n" + "meanTH=" + QString::number(m);
			painter.drawText(qImg.rect(), QTextOption::WrapAtWordBoundaryOrAnywhere || Qt::AlignTop || Qt::AlignLeft, outText);
			magImg = Image::qImage2Mat(qImg);
			cv::cvtColor(magImg, magImg, cv::COLOR_BGR2GRAY);

			cv::Mat normAccPMF;
			accPMFImg = cv::Mat::zeros(255, accPMF.cols, CV_8U);
			normalize(accPMF, normAccPMF, 0, 255, CV_MINMAX);
			normAccPMF.convertTo(normAccPMF, CV_8U);

			for (int j = 0; j < accPMFImg.cols; j++) {
				cv::Point p1 = cv::Point(j, accPMFImg.rows - 1);
				cv::Point p2 = cv::Point(j, (accPMFImg.rows - 1) - normAccPMF.at<uchar>(j));
				cv::line(accPMFImg, p1, p2, cv::Scalar(255), 1, 8, 0);
			}

			nacImages << nacLayer;
			magImages << magImg;
			//debug draw-------------------------------------------------------------------------------------------------------------------------
		}

		return accPMF;
	}

	cv::Mat TextHeightEstimation::nacf(const cv::Mat patchImg) {

		cv::Mat dftInput, dftResult, autoCorr, normAutoCorr;
		patchImg.convertTo(dftInput, CV_32F);

		//compute auto correlation
		dft(dftInput, dftResult, cv::DFT_COMPLEX_OUTPUT);
		cv::mulSpectrums(dftResult, dftResult, autoCorr, 0, true);
		dft(autoCorr, autoCorr, cv::DFT_INVERSE | cv::DFT_SCALE | cv::DFT_REAL_OUTPUT);

		//normalize result		
		normalize(autoCorr, normAutoCorr, 0, 1, CV_MINMAX);

		//shift result image (since zero frequency comp was not centered)
		fftShift(normAutoCorr);

		return normAutoCorr;
	}

	cv::Mat TextHeightEstimation::computePMF(int tMax, double w, double m, double sig) {
		cv::Mat pmf = cv::Mat::zeros(1, tMax, CV_32F);

		for (int t = 0; t < tMax; ++t){
			double expVal = exp(-0.5*pow((t - m), 2) / sig);
			double ft = w * expVal;
			pmf.at<float>(t) = ft;
		}

		return pmf;
	}

	//shifting zero frequency components to center of image, using code from opencv phase correlation function
	//https://github.com/opencv/opencv/blob/2da96be217ab437de854aca7f7670c2048fb554b/modules/imgproc/src/phasecorr.cpp
	void TextHeightEstimation::fftShift(cv::Mat out){

		if (out.rows == 1 && out.cols == 1)
			return;

		std::vector<cv::Mat> planes;
		split(out, planes);

		int xMid = out.cols >> 1;
		int yMid = out.rows >> 1;

		bool is_1d = xMid == 0 || yMid == 0;

		if (is_1d){
			int is_odd = (xMid > 0 && out.cols % 2 == 1) || (yMid > 0 && out.rows % 2 == 1);
			xMid = xMid + yMid;

			for (size_t i = 0; i < planes.size(); i++){
				cv::Mat tmp;
				cv::Mat half0(planes[i], cv::Rect(0, 0, xMid + is_odd, 1));
				cv::Mat half1(planes[i], cv::Rect(xMid + is_odd, 0, xMid, 1));

				half0.copyTo(tmp);
				half1.copyTo(planes[i](cv::Rect(0, 0, xMid, 1)));
				tmp.copyTo(planes[i](cv::Rect(xMid, 0, xMid + is_odd, 1)));
			}
		}
		else{
			int isXodd = out.cols % 2 == 1;
			int isYodd = out.rows % 2 == 1;
			for (size_t i = 0; i < planes.size(); i++){
				// perform quadrant swaps...
				cv::Mat q0(planes[i], cv::Rect(0, 0, xMid + isXodd, yMid + isYodd));
				cv::Mat q1(planes[i], cv::Rect(xMid + isXodd, 0, xMid, yMid + isYodd));
				cv::Mat q2(planes[i], cv::Rect(0, yMid + isYodd, xMid + isXodd, yMid));
				cv::Mat q3(planes[i], cv::Rect(xMid + isXodd, yMid + isYodd, xMid, yMid));

				if (!(isXodd || isYodd)){
					cv::Mat tmp;
					q0.copyTo(tmp);
					q3.copyTo(q0);
					tmp.copyTo(q3);

					q1.copyTo(tmp);
					q2.copyTo(q1);
					tmp.copyTo(q2);
				}
				else{
					cv::Mat tmp0, tmp1, tmp2, tmp3;
					q0.copyTo(tmp0);
					q1.copyTo(tmp1);
					q2.copyTo(tmp2);
					q3.copyTo(tmp3);

					tmp0.copyTo(planes[i](cv::Rect(xMid, yMid, xMid + isXodd, yMid + isYodd)));
					tmp3.copyTo(planes[i](cv::Rect(0, 0, xMid, yMid)));

					tmp1.copyTo(planes[i](cv::Rect(0, yMid, xMid, yMid + isYodd)));
					tmp2.copyTo(planes[i](cv::Rect(xMid, 0, xMid + isXodd, yMid)));
				}
			}
		}

		merge(planes, out);
	}

	void TextHeightEstimation::computeConfidence(cv::Mat accPMF){

		//compute standard deviation of accumulated PMF -> giving approximate confidence of THE results
		cv::Mat xi = cv::Mat::zeros(accPMF.size(), CV_32F);
		for (int i = 0; i < accPMF.cols; ++i) {
			xi.at<float>(i) = i;
		}

		double sumVal = cv::sum(accPMF)[0];
		cv::Mat normPMF = accPMF / sumVal;

		cv::Mat mi = xi.mul(normPMF);
		double m = cv::sum(mi)[0];

		xi = xi - m;
		xi = (xi).mul(xi);
		xi = xi.mul(normPMF);

		double sig = cv::sum(xi)[0];
		sig = sqrt(sig);

		theConfidence = 1 - (sig / m);
	}

	int TextHeightEstimation::textHeightEstimate() {
		return thEstimate;
	}

	double TextHeightEstimation::coverage(){
		return std::floor(meanCoverage * 100) / 100.0;
	}

	double TextHeightEstimation::relCoverage(){
		return std::floor(meanRelativeCoverage * 100) / 100.0;
	}

	double TextHeightEstimation::confidence(){
		return std::floor(theConfidence * 100) / 100.0;
	}

	cv::Mat TextHeightEstimation::draw(const cv::Mat & img, const QColor& col) const {

		QImage qImg = Image::mat2QImage(img, true);
		QPainter painter(&qImg);

		painter.drawRect(QRect(0,0,thEstimate,thEstimate));
		QString outText = "th=" + QString::number(thEstimate) + "\n" + "conf=" + QString::number(theConfidence) + "\n" + "cover=" + QString::number(meanCoverage) + "\n" + "relCover=" + QString::number(meanRelativeCoverage);
		//QString outText = "th=" + QString::number(thEstimate) + "\n" + "conf=" + QString::number(theConfidence);
		painter.drawText(QRect(0, 0, thEstimate, thEstimate), QTextOption::WrapAtWordBoundaryOrAnywhere || Qt::AlignTop || Qt::AlignLeft, outText);

		return Image::qImage2Mat(qImg);
	}

	void TextHeightEstimation::drawDebugImages(QString input_path) const{
		
		input_path = QDir::cleanPath(input_path);
		QFileInfo pathInfo(input_path);
		pathInfo = QFileInfo(input_path);

		if (!pathInfo.exists()) {
			qWarning() << "Output path for debug images does not exist.";
			return;
		} else {
			if (pathInfo.isDir())
				input_path += "/result.png";
		}

		if (accPMFImg.empty() || magImages.isEmpty() || nacImages.isEmpty()) {
			qWarning() << "Debug images could not be found. Make sure the 'debugDraw' option is set to true and the results have been computed in advance.";
			return;
		}

		QString debugPath = Utils::createFilePath(input_path, "_THE_accPMF", ".png");
		Image::save(accPMFImg, debugPath);

		for (int i = 0; i < nacImages.size(); ++i) {
			debugPath = Utils::createFilePath(input_path, "_THE_nac"+QString::number(i), ".png");
			Image::save(nacImages[i], debugPath);
		}

		for (int i = 0; i < magImages.size(); ++i) {
			debugPath = Utils::createFilePath(input_path, "_THE_mag" + QString::number(i), ".png");
			Image::save(magImages[i], debugPath);
		}
	}

	QString TextHeightEstimation::toString() const {
		return Module::toString();
	}

	bool TextHeightEstimation::checkInput() const {
		return !isEmpty();
	}

	QSharedPointer<TextHeightEstimationConfig> TextHeightEstimation::config() const {
		return qSharedPointerDynamicCast<TextHeightEstimationConfig>(mConfig);
	}
}