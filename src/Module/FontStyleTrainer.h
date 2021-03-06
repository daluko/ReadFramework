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

#include "BaseModule.h"
#include "PixelSet.h"
#include "SuperPixelTrainer.h"
#include "FontStyleClassification.h"

#pragma warning(push, 0)	// no warnings from includes

#pragma warning(pop)

#ifndef DllCoreExport
#ifdef DLL_CORE_EXPORT
#define DllCoreExport Q_DECL_EXPORT
#else
#define DllCoreExport Q_DECL_IMPORT
#endif
#endif

namespace rdf {

class DllCoreExport FontStyleTrainerConfig : public ModuleConfig {

public:
	FontStyleTrainerConfig();

	virtual QString toString() const override;
	
	QString modelPath() const;
	int modelType() const;
	int defaultK() const;

	void setModelPath(const QString &modelPath);
	void setModelType(int modelType);
	void setDefaultK(int defaultK);

protected:

	QString mModelPath;
	int mModelType = FontStyleClassifier::classify_bayes;
	int mDefaultK = 30;

	void load(const QSettings& settings) override;
	void save(QSettings& settings) const override;
};

class DllCoreExport FontStyleTrainer : public Module {

public:
	FontStyleTrainer(FontStyleDataSet dataSet);

	bool isEmpty() const override;
	bool compute() override;
	QSharedPointer<FontStyleTrainerConfig> config() const;

	cv::Mat draw(const cv::Mat& img) const;
	QString toString() const override;

	// no read function -> see FontStyleClassifier
	bool write(const QString& filePath) const;
	QSharedPointer<FontStyleClassifier> classifier() const;

private:
	FeatureCollectionManager mFeatureManager;
	GaborFilterBank mGFB;
	int mPatchHeight = -1;

	// results
	cv::Ptr<cv::ml::StatModel> mModel;
	int mModelType = FontStyleClassifier::classify_nn_wed;

	bool checkInput() const override;
};

}
