#include "GaborFiltering.h"
#include "GaborFiltering.h"
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

#include "WhiteSpaceAnalysis.h"
#include "GaborFiltering.h"
#include "FontStyleClassification.h"

#pragma warning(push, 0)	// no warnings from includes
#include <opencv2/imgproc.hpp>
#include <QJsonArray>
#pragma warning(pop)

namespace rdf {

// Gabor Filtering --------------------------------------------------------------------

cv::Mat GaborFiltering::createGaborKernel(int kSize, double lambda, double theta, double sigma) {
	
	cv::Mat kernel;
	double kh = kSize / 2.0;

	cv::Mat hek = cv::Mat(kSize, kSize, CV_32F);
	cv::Mat hok = cv::Mat(kSize, kSize, CV_32F);

	for (double y = -kh; y < kh; y++) {
		for (double x = -kh; x < kh; x++) {

			double val1 = std::exp(  -1 * (std::pow(x, 2) + std::pow(y, 2)) / (2*std::pow(sigma, 2)) );
			double val2 = (2*CV_PI*(1/lambda)) * (x*std::cos(theta) + y*sin(theta));
			
			double Ve = std::cos(val2);
			double Vo = std::sin(val2);
			
			double he = val1 * Ve;
			double ho = val1 * Vo;

			int y_ = (int) y + (int) kh;
			int x_ = (int) x + (int) kh;

			hek.at<float>(y_, x_) = (float)he;
			hok.at<float>(y_, x_) = (float)ho;
		}
	}

	cv::merge(std::vector<cv::Mat>{hek, hok}, kernel);

	return kernel;
}

QVector<cv::Mat> GaborFiltering::createGaborKernels(QVector<double> lambda, QVector<double> theta,
	int kSize, double sigmaMultiplier, double psi, double gamma, bool openCV) {

	QVector<cv::Mat> kernels = QVector<cv::Mat>();

	for (int l = 0; l < lambda.length(); ++l) {
		for (int t = 0; t < theta.length(); ++t) {
			
			cv::Mat kernel;
			
			double sigma_; //note that by default sigma is chosen as the inverse of the frequency (=wavelength(lambda))
			if (sigmaMultiplier == -1) {
				sigma_ = lambda[l];
			}
			else
				sigma_ = lambda[l]/ sigmaMultiplier; //sigma_ = 1/f*sigmaMultiplier

			//qDebug() << "lambda = " << lambda[l] << ", theta = " << theta[t] << ", sigma = " << sigma_;

			//gabor kernel creation using OpenCV implementation	
			if (openCV) {
				//get real and imaginary part of kernel
				cv::Mat kernel_real = cv::getGaborKernel(cv::Size(kSize, kSize), sigma_, theta[t], lambda[l], gamma, psi, CV_32F);
				cv::Mat kernel_img = cv::getGaborKernel(cv::Size(kSize, kSize), sigma_, theta[t], lambda[l], gamma, psi+(CV_PI*0.5), CV_32F);

				cv::merge(std::vector<cv::Mat>{kernel_real, kernel_img}, kernel);
			}
			else {
				//get even and odd gabor kernels 
				kernel = GaborFiltering::createGaborKernel(kSize, lambda[l], theta[t], sigma_);
			}

			kernels << kernel;
		}
	}


	return kernels;	
}

cv::Mat GaborFiltering::extractGaborFeatures(cv::Mat img_in, GaborFilterBank filterBank) {
	
	//convert input matrix for processing
	cv::Mat img;
	if (img_in.channels() == 1)
		img = img_in.clone();
	else
		cv::cvtColor(img_in, img, CV_BGR2GRAY);

	if (img.type() != CV_32F)
		img.convertTo(img, CV_32F);


	cv::Mat featVec;

	for (auto kernel : filterBank.kernels()) {
		cv::Mat imgFiltered_real, imgFiltered_img;
		
		cv::Mat kernel_planes[2];
		cv::split(kernel, kernel_planes);

		filter2D(img, imgFiltered_real, CV_32F, kernel_planes[0]);
		filter2D(img, imgFiltered_img, CV_32F, kernel_planes[1]);

		//get magnitude of signal
		cv::Mat magnitudeResult;
		cv::magnitude(imgFiltered_real, imgFiltered_img, magnitudeResult);

		cv::Mat imgNorm;
		normalize(magnitudeResult, imgNorm, 0, 255, CV_MINMAX);
		imgNorm.convertTo(imgNorm, CV_8UC1);

		//compute mean + std 
		cv::Scalar mean, stddev;
		meanStdDev(imgNorm, mean, stddev);

		featVec.push_back(mean.val[0]);
		featVec.push_back(stddev.val[0]);

		//qDebug() << "mean" << mean.val[0];
		//qDebug() << "stdev" << stddev.val[0];
	}

	return featVec;
}

GaborFilterBank::GaborFilterBank(){
	
	mLambda = QVector<double>();
	mTheta = QVector<double>();
	mKernels = GaborFiltering::createGaborKernels(mLambda, mTheta, mKernelSize);
}

GaborFilterBank::GaborFilterBank(QVector<double> lambda, QVector<double> theta, int kernelSize, double sigmaMultiplier){
	setLambda(lambda);
	setKernelSize(kernelSize);
	setTheta(theta);
	setSigmaMultiplier(sigmaMultiplier);

	for (int i = 0; i < mTheta.size(); i++)
		mTheta[i] *= DK_DEG2RAD;

	mKernels = GaborFiltering::createGaborKernels(mLambda, mTheta, mKernelSize, sigmaMultiplier);
}

void GaborFilterBank::setLambda(QVector<double> lambda){
	mLambda = lambda;
}

void GaborFilterBank::setTheta(QVector<double> theta){
	mTheta = theta;
}

void GaborFilterBank::setSigmaMultiplier(double sigmaMultiplier){
	mSigmaMultiplier = sigmaMultiplier;
}

void GaborFilterBank::setKernelSize(int kernelSize){
	mKernelSize = kernelSize;
}

void GaborFilterBank::setKernels(QVector<cv::Mat> kernels) {
	mKernels = kernels;
}

QVector<double> GaborFilterBank::lambda() const{
	return mLambda;
}

QVector<double> GaborFilterBank::theta() const{
	return mTheta;
}

double GaborFilterBank::sigmaMultiplier() const{
	return mSigmaMultiplier;
}

QVector<cv::Mat> GaborFilterBank::kernels() const {
	return mKernels;
}

int GaborFilterBank::kernelSize() const{
	return mKernelSize;
}

bool GaborFilterBank::isEmpty() const{
	return mKernels.isEmpty();
}

QVector<cv::Mat> GaborFilterBank::draw(){
	
	if (mLambda.length()*mTheta.length() != mKernels.size()) {
		qWarning() << "Unable to print filter bank.";
		qWarning() << "Please make sure the number of parameter combinations matches the number of kernels.";
		return QVector<cv::Mat>();
	}
	
	cv::Mat kernelImages;
	cv::Mat kernelDFTImages;
	cv::Mat filterBankFR;

	for (int l = 0; l < mLambda.length(); ++l) {
		cv::Mat kernelRow, kernelDFTRow;
		for (int t = 0; t < mTheta.length(); ++t) {

			int idx = (l* mTheta.length()) + t;
			cv::Mat kernel = mKernels[idx];
			cv::Mat kernel_planes[2];
			cv::split(kernel, kernel_planes);
			kernel = kernel_planes[0];		//visualize real part of kernel only

			// visualize kernels in spatial domain
			cv::Mat kernel_norm = kernel.clone();
			normalize(kernel_norm, kernel_norm, 0, 1, CV_MINMAX);

			if (kernelRow.empty()) {
				QImage qImg = QImage(200, kernel_norm.size().height, QImage::Format_Grayscale8);
				qImg.fill(QColor(255,255,255));
				QPainter painter(&qImg);

				QString outText = "lambda/f = " + QString::number(mLambda[l]);
				painter.drawText(QRect(0, 0, 200, kernel_norm.size().height), Qt::AlignCenter, outText);
				kernelRow = Image::qImage2Mat(qImg);
				kernelRow.convertTo(kernelRow, CV_32F);
				
				//insert kernel image
				cv::hconcat(kernelRow, kernel_norm, kernelRow);
			}
			else {
				cv::hconcat(kernelRow, cv::Mat::ones(kernelRow.size().height, 5, CV_32F), kernelRow);
				cv::hconcat(kernelRow, kernel_norm, kernelRow);
			}
			
			// compute and visualize DFT of kernel (frequency domain visualization)
			cv::Mat kernelDFT, magnitudeImg;
			dft(kernel, kernelDFT, cv::DFT_COMPLEX_OUTPUT);

			std::vector<cv::Mat> planes;
			split(kernelDFT, planes);							// planes[0] = Re(DFT(I)), planes[1] = Im(DFT(I))
			magnitude(planes[0], planes[1], magnitudeImg);
			
			magnitudeImg += cv::Scalar::all(1);					//switch to logarithmic scale
			log(magnitudeImg, kernelDFT);
			WSAHelper::fftShift(kernelDFT);						// shift 0 frequency to center of image
			normalize(kernelDFT, kernelDFT, 0, 1, CV_MINMAX);

			if (filterBankFR.empty()) {
				filterBankFR = kernelDFT.clone();
			}
			else {
				cv::Mat mask = kernelDFT > filterBankFR;
				kernelDFT.copyTo(filterBankFR, mask);
			}
				

			if (kernelDFTRow.empty()) {
				kernelDFTRow = kernelDFT;
			}
			else {
				cv::hconcat(kernelDFTRow, cv::Mat::ones(kernelDFTRow.size().height, 5, CV_32F), kernelDFTRow);
				cv::hconcat(kernelDFTRow, kernelDFT, kernelDFTRow);
			}
		}

		if (kernelImages.empty())
			kernelImages = kernelRow.clone();
		else {
			cv::vconcat(kernelImages, cv::Mat::ones(5, kernelImages.size().width, CV_32F), kernelImages);
			cv::vconcat(kernelImages, kernelRow.clone(), kernelImages);
		}

		if (kernelDFTImages.empty())
			kernelDFTImages = kernelDFTRow.clone();
		else {
			cv::vconcat(kernelDFTImages, cv::Mat::ones(5, kernelDFTImages.size().width, CV_32F), kernelDFTImages);
			cv::vconcat(kernelDFTImages, kernelDFTRow.clone(), kernelDFTImages);
		}
	}

	cv::Mat topRow;
	int width = mKernels[0].cols;

	for (int t = 0; t < mTheta.length(); ++t) {

		QImage qImg = QImage(width, 50, QImage::Format_Grayscale8);
		qImg.fill(QColor(255, 255, 255));
		QPainter painter(&qImg);

		QString outText = "theta = " + QString::number(mTheta[t]);
		painter.drawText(QRect(0, 0, width, 50), Qt::AlignCenter, outText);

		cv::Mat topRow_ = Image::qImage2Mat(qImg);
		topRow_.convertTo(topRow_, CV_32F);
		
		if (topRow.empty()) {
			topRow = cv::Mat::ones(50, 200, CV_32F);
			cv::hconcat(topRow, topRow_, topRow);
		}
		else {
			cv::hconcat(topRow, cv::Mat::ones(50, 5, CV_32F), topRow);
			cv::hconcat(topRow, topRow_, topRow);
		}		
	}

	cv::vconcat(topRow, kernelImages, kernelImages);

	QVector<cv::Mat> output = {kernelImages, kernelDFTImages, filterBankFR };

	return output;
}

QString GaborFilterBank::toString(){

	QString gfb_params = "Gabor filter bank parameters \n lambda = {";

	for (auto l : mLambda)
		gfb_params += QString::number(l) + "; ";
	
	gfb_params.chop(2);
	gfb_params += "} \n theta = { ";

	for (auto t : mTheta)
		gfb_params += QString::number(t/DK_DEG2RAD) + "; ";

	gfb_params.chop(2);
	gfb_params += "}";

	gfb_params += " \n kernelSize = " + QString::number(mKernelSize) + "; ";
	gfb_params += " \n sigmaMultiplier = " + QString::number(mSigmaMultiplier) + "; ";

	return gfb_params;
}

void GaborFilterBank::toJson(QJsonObject & jo) const{

	QJsonObject joc;
	
	joc.insert("kernelSize", QJsonValue(mKernelSize));
	joc.insert("sigmaMultiplier", QJsonValue(mSigmaMultiplier));

	QJsonArray ja;
	for (double t : mTheta)
		ja.append(t*DK_RAD2DEG);

	joc.insert("theta", ja);

	QJsonArray ja1;
	for (double l : mLambda)
		ja1.append(l);

	joc.insert("lambda", ja1);
	jo.insert(jsonKey(), joc);
}

GaborFilterBank GaborFilterBank::fromJson(QJsonObject & jo){
	int kernelSize = jo.value("textureSize").toInt(128);
	double sigmaMultiplier = jo.value("sigmaMultiplier").toDouble(-1);

	QVector<double> theta;
	for (const QJsonValue& jv : jo.value("theta").toArray()) {
		const double t = jv.toDouble(-1);
		if (t != -1)
			theta << t;
	}

	QVector<double> lambda;
	for (const QJsonValue& jv : jo.value("lambda").toArray()) {
		const double l = jv.toDouble(-1);
		if (l != -1)
			lambda << l;
	}

	GaborFilterBank gfb = GaborFilterBank(lambda, theta, kernelSize, sigmaMultiplier);

	return gfb;
}

GaborFilterBank GaborFilterBank::read(QString filePath){

	QJsonObject jo = Utils::readJson(filePath).value(GaborFilterBank::jsonKey()).toObject();
	GaborFilterBank gfb = fromJson(jo);

	return gfb;
}

QString GaborFilterBank::jsonKey(){
	return "GaborFilterBank";
}

}