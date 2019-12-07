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

#include "FontStyleTrainer.h"

#pragma warning(push, 0)	// no warnings from includes
#include <QPainter>
#pragma warning(pop)

namespace rdf {

// FontStyleTrainerConfig --------------------------------------------------------------------
FontStyleTrainerConfig::FontStyleTrainerConfig() : ModuleConfig("FontStyleTrainer") {
}

QString FontStyleTrainerConfig::toString() const {
	QString msg = ModuleConfig::toString();
	return msg;
}

QString FontStyleTrainerConfig::modelPath() const {
	return mModelPath;
}

int FontStyleTrainerConfig::modelType() const {
	return mModelType;
}

int FontStyleTrainerConfig::defaultK() const{
	return mDefaultK;
}

void FontStyleTrainerConfig::setModelPath(const QString & modelPath) {
	mModelPath = modelPath;
}

void FontStyleTrainerConfig::setModelType(int modelType) {
	mModelType = modelType;
}

void FontStyleTrainerConfig::setDefaultK(int defaultK) {
	mDefaultK = defaultK;
}

void FontStyleTrainerConfig::load(const QSettings & settings) {
	mModelPath	= settings.value("modelPath", modelPath()).toString();
	mModelType	= settings.value("modelType", modelType()).toInt();
	mDefaultK	= settings.value("defaultK", defaultK()).toInt();
}

void FontStyleTrainerConfig::save(QSettings & settings) const {
	settings.setValue("modelPath", modelPath());
	settings.setValue("modelType", modelType());
	settings.setValue("defaultK", defaultK());
}

// FontStyleTrainer --------------------------------------------------------------------
FontStyleTrainer::FontStyleTrainer(FontStyleDataSet dataSet) {
	mGFB = dataSet.gaborFilterBank();
	mFeatureManager = dataSet.featureCollectionManager();
	mConfig = QSharedPointer<FontStyleTrainerConfig>::create();
	mConfig->loadSettings();
}

bool FontStyleTrainer::isEmpty() const {
	return mFeatureManager.isEmpty();
}

bool FontStyleTrainer::compute() {

	Timer dt;
	
	if (!checkInput()) {
		qCritical() << "Failed to train classifier. Found no feature vectors.";
		return false;
	}

	//create opencv stat model depneding on model type
	mModelType = config()->modelType();
	if (mModelType == FontStyleClassifier::classify_bayes) {
		auto model = cv::ml::NormalBayesClassifier::create();
		mInfo << "Training Bayes model with" << mFeatureManager.numFeatures() << "features, this might take a while...";
		model->train(mFeatureManager.toCvTrainData(-1, false));
		mModel = model;
	}
	else if (mModelType == FontStyleClassifier::classify_svm) {
		//auto model = cv::ml::SVM::create();
		//mInfo << "Training SVM model with" << mFeatureManager.numFeatures() << "features, this might take a while...";
		//model->train(mFeatureManager.toCvTrainData(-1, false));
		//mModel = model;
		qCritical() << "Failed to train classifier. SVM classification mode not implemented";
		return false;
	}
	else{
		auto model = cv::ml::KNearest::create();
		cv::Ptr<cv::ml::TrainData> trainData;

		if (mModelType == FontStyleClassifier::classify_knn) {
			int k = config()->defaultK();
			model->setDefaultK(k);
			trainData = mFeatureManager.toCvTrainData(-1, false);
		}
		else {
			//compute adapted training set ((weighted) centroids)
			auto centroids = mFeatureManager.collectionCentroids();
			auto collections = mFeatureManager.collection();
			cv::Mat centroidsMat;
			cv::Mat labelMat;

			for (int i = 0; i < collections.size(); ++i) {
				labelMat.push_back<int>(collections[i].label().id());
				centroidsMat.push_back(centroids[i]);
			}

			model->setDefaultK(1);	//use k=1 -> nearest neighbor

			if (mModelType == FontStyleClassifier::classify_nn) {
				trainData = cv::ml::TrainData::create(centroidsMat, cv::ml::ROW_SAMPLE, labelMat);
			}
			else {
				if (mModelType != FontStyleClassifier::classify_nn_wed) {
					mModelType = FontStyleClassifier::classify_nn_wed;
					qWarning() << "Unable to identify font style classifier mode.";
					qInfo() << "Font style classifier mode set to: Nearest neighbor using weighted euclidean distance.";
				}
				
				cv::Mat featSTD = mFeatureManager.featureSTD();
				for (int i = 0; i < centroidsMat.rows; ++i) {
					cv::Mat row = centroidsMat.row(i);
					row = row / featSTD;
				}

				trainData = cv::ml::TrainData::create(centroidsMat, cv::ml::ROW_SAMPLE, labelMat);
			}
		}

		model->train(trainData);
		mModel = model;
	}

	if (!mModel) {
		qCritical() << "Failed to train font style classifier.";
		return false;
	}

	if (!mModel->isTrained()) {
		qCritical() << "Failed to train font style classifier.";
		return false;
	}

	mInfo << "Trained font style classifier in " << dt;

	return true;
}

QSharedPointer<FontStyleTrainerConfig> FontStyleTrainer::config() const {
	return qSharedPointerDynamicCast<FontStyleTrainerConfig>(Module::config());
}

cv::Mat FontStyleTrainer::draw(const cv::Mat & img) const {

	// draw mser blobs
	Timer dtf;
	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);
	// TODO: draw something

	return Image::qImage2Mat(qImg);
}

QString FontStyleTrainer::toString() const {
	return config()->toString();
}

bool FontStyleTrainer::write(const QString & filePath) const {

	if (mModel && !mModel->isTrained())
		qWarning() << "writing classifier that is NOT trained!";

	return classifier()->write(filePath);
}

QSharedPointer<FontStyleClassifier> FontStyleTrainer::classifier() const {
	QSharedPointer<FontStyleClassifier> sm(new FontStyleClassifier(FontStyleDataSet(mFeatureManager, mPatchHeight, mGFB), mModel, mModelType));
	return sm;
}

bool FontStyleTrainer::checkInput() const {
	return !isEmpty();
}

}