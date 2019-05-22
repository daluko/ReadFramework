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

#pragma warning(push, 0)	// no warnings from includes
#include <opencv2/imgproc.hpp>
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

			int y_ = (int) y + kh;
			int x_ = (int) x + kh;

			hek.at<float>(y_, x_) = (float)he;
			hok.at<float>(y_, x_) = (float)ho;
		}
	}

	cv::merge(std::vector<cv::Mat>{hek, hok}, kernel);

	return kernel;
}


GaborFilterBank GaborFiltering::createGaborFilterBank(QVector<double> lambda, QVector<double> theta,
	int ksize, double sigma, double psi, double gamma, bool openCV) {

	QVector<cv::Mat> kernels;

	for (int l = 0; l < lambda.length(); ++l) {
		for (int t = 0; t < theta.length(); ++t) {
			
			cv::Mat kernel;

			double sigma_;
			if (sigma == -1)
				sigma_ = lambda[l];
			else
				sigma_ = sigma;

			//qDebug() << "lambda = " << lambda[l] << ", theta = " << theta[t] << ", sigma = " << sigma_;

			//gabor kernel creation using OpenCV implementation	
			if (openCV) {
				//get real and imaginary part of kernel
				cv::Mat kernel_real = cv::getGaborKernel(cv::Size(ksize, ksize), sigma_, theta[t], lambda[l], gamma, psi, CV_32F);
				cv::Mat kernel_img = cv::getGaborKernel(cv::Size(ksize, ksize), sigma_, theta[t], lambda[l], gamma, psi+(CV_PI*0.5), CV_32F);

				cv::merge(std::vector<cv::Mat>{kernel_real, kernel_img}, kernel);
			}
			else {
				//get even and odd gabor kernels 
				kernel = GaborFiltering::createGaborKernel(ksize, lambda[l], theta[t], sigma_);
			}

			kernels << kernel;
		}
	}

	return GaborFilterBank(lambda, theta, kernels);	
}

cv::Mat GaborFiltering::extractGaborFeatures(cv::Mat img_in, GaborFilterBank filterBank) {

	//TODO check if normalization is needed
	
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
	mKernels = QVector<cv::Mat>();
	mLambda = QVector<double>();
	mTheta = QVector<double>();
}

GaborFilterBank::GaborFilterBank(QVector<double> lambda, QVector<double> theta, QVector<cv::Mat> kernels){
	mKernels = kernels;
	mLambda = lambda;
	mTheta = theta;
}

void GaborFilterBank::setLambda(QVector<double> lambda){
	mLambda = lambda;
}

void GaborFilterBank::setTheta(QVector<double> theta){
	mTheta = theta;
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

QVector<cv::Mat> GaborFilterBank::kernels() const {
	return mKernels;
}

bool GaborFilterBank::isEmpty(){
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

	for (int l = 0; l < mLambda.length(); ++l) {
		cv::Mat kernelRow, kernelDFTRow;
		for (int t = 0; t < mTheta.length(); ++t) {

			int idx = (l* mTheta.length()) + t;
			cv::Mat kernel = mKernels[idx];
			cv::Mat kernel_planes[2];
			cv::split(kernel, kernel_planes);
			kernel = kernel_planes[1];	//visualize real part of kernel only

			// visualize kernels in spatial domain
			if (kernelRow.empty()) {
				cv::Mat kernel_norm = kernel.clone();
				normalize(kernel_norm, kernel_norm, 0, 1, CV_MINMAX);
				kernelRow = kernel_norm;
			}
			else {
				cv::hconcat(kernelRow, cv::Mat::ones(kernelRow.size().height, 5, CV_32F), kernelRow);

				cv::Mat kernel_norm = kernel.clone();
				normalize(kernel_norm, kernel_norm, 0, 1, CV_MINMAX);
				cv::hconcat(kernelRow, kernel_norm, kernelRow);
			}
			
			// compute and visualize DFT of kernel (frequency domain visualization)
			cv::Mat kernelDFT, magnitudeImg;
			dft(kernel, kernelDFT, cv::DFT_COMPLEX_OUTPUT);

			std::vector<cv::Mat> planes;
			split(kernelDFT, planes);							// planes[0] = Re(DFT(I), planes[1] = Im(DFT(I))
			magnitude(planes[0], planes[1], magnitudeImg);		// planes[0] = magnitude
			
			magnitudeImg += cv::Scalar::all(1);					//switch to logarithmic scale
			log(magnitudeImg, kernelDFT);
			WSAHelper::fftShift(kernelDFT);						// shift 0 frequency to center of image

			if (kernelDFTRow.empty()) {
				normalize(kernelDFT, kernelDFT, 0, 1, CV_MINMAX);
				kernelDFTRow = kernelDFT;
			}
			else {
				cv::hconcat(kernelDFTRow, cv::Mat::ones(kernelDFTRow.size().height, 5, CV_32F), kernelDFTRow);

				normalize(kernelDFT, kernelDFT, 0, 1, CV_MINMAX);
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

	QVector<cv::Mat> output = {kernelImages, kernelDFTImages};

	return output;
}

}