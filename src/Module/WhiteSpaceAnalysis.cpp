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

#include "WhiteSpaceAnalysis.h"
#include "Image.h"
#include "Drawer.h"
#include "SuperPixelScaleSpace.h"
#include "SuperPixelClassification.h"
#include "ScaleFactory.h"

#include <vector>
#include <algorithm>

#pragma warning(push, 0)	// no warnings from includes
#include <QSettings>
#include <QDebug>
#include <QPainter>
#include <QFileInfo>
#pragma warning(pop)

 // Qt defines

namespace rdf {

	// read defines

// WhiteSpaceConfig --------------------------------------------------------------------
	WhiteSpaceAnalysisConfig::WhiteSpaceAnalysisConfig() : ModuleConfig("White Space Analysis Module") {
	}

	QString WhiteSpaceAnalysisConfig::toString() const {
		return ModuleConfig::toString();
	}

	void WhiteSpaceAnalysisConfig::setShowResults(bool show) {
		mShowResults = show;
	}

	bool WhiteSpaceAnalysisConfig::ShowResults() const {
		return mShowResults;
	}

	void WhiteSpaceAnalysisConfig::setMinRectsPerSpace(int minRects) {
		mMinRectsPerSpace = minRects;
	}

	int WhiteSpaceAnalysisConfig::minRectsPerSpace() const {
		return ModuleConfig::checkParam(mMinRectsPerSpace, 0, INT_MAX, "minRectsPerSpace");
	}

	void WhiteSpaceAnalysisConfig::load(const QSettings & settings) {

		mShowResults = settings.value("ShowResults", ShowResults()).toBool();
		mMinRectsPerSpace = settings.value("minRectsPerSpace", minRectsPerSpace()).toInt();
	}

	void WhiteSpaceAnalysisConfig::save(QSettings & settings) const {

		settings.setValue("ShowResults", ShowResults());
		settings.setValue("minRectsPerSpace", minRectsPerSpace());
	}

// Module --------------------------------------------------------------------
WhiteSpaceAnalysis::WhiteSpaceAnalysis(const cv::Mat& img) {

	mImg = img;
	//ScaleFactory::instance().init(img.size());

	mConfig = QSharedPointer<WhiteSpaceAnalysisConfig>::create();
mConfig->loadSettings();
}

bool WhiteSpaceAnalysis::isEmpty() const {
	return mImg.empty();
}

bool WhiteSpaceAnalysis::compute() {
	qDebug() << "computing white spaces...";

	if (!checkInput())
		return false;

	cv::Mat img = mImg;
	Timer dt;

	//TODO use superpixels instead of mserBlobs
	//TODO use adapted version of SuperPixels instead of textComponents
	//TODO use pixel connectors and existing code, avoid mixture of data structures and algorithms

	//TODO improve initial set of components used for text line formation 
	//img = ScaleFactory::scaled(img);
	//ScaleSpaceSuperPixel<GridSuperPixel> sp(img);

	//if (!sp.compute()) {
	//	mWarning << "could not compute super pixels!";
	//	return false;
	//}

	SuperPixel sp = computeSuperPixels(img);

	//convert super pixels (sp) to text components (tc)
	mBlobs = sp.getMserBlobs();

	extractTextComponents();
	computeInitialTLC();	//create initial text line candidates

	//compute white space for each tlc
	for (auto tlc : mTlcM) {
		tlc->computeWhiteSpaces(mImg.cols);
	}

	findBCR();		//mark between column rectangles (ws), not surrounded by text components
	splitTLC();		//split the tlc according to the bcr and create a list of the remaining ws
	groupWS();		//form runs of white spaces

	updateBCR();	//check if bcr are wider than the ws of their neighboring tlc
	while (updateSegmentation()) {
		updateBCR();
	}

	////remove leading or trailing within column candidates
	//bool updated = false;
	//for (auto wsr : mWsrM) {
	//	if (trimWSR(wsr))
	//		updated = true;
	//}

	//if (updated) {
	//	updateBCR();
	//	while (updateSegmentation()) {
	//		updateBCR();
	//	}
	//}

	//TODO check if sorting is necessary
	//sort text lines according to y coordinate
	std::sort(mTlcM.begin(), mTlcM.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().bottom() < rhs->bbox().bottom();
	});

	convertTLC();	//convert tlc to textRegion objects

	TextBlockFormation tbf(mTextLines);
	if (!tbf.compute()) {
		qWarning() << "White space analysis: Could not compute text blocks!";
		return false;
	}

	mTextBlocks = tbf.textBlocks();

	//for (int i = 0; i < mTlcM.length(); ++i){
	//	qInfo() << i << ": bbox = " << mTlcM.at(i)->bbox().toString() << " | annCount= " << annCount[i] << " | bnnIndices= " << bnnIndices[i];
	//}

	// draw debug image-------------------------------------------------------------------
	//QImage qImg1 = Image::mat2QImage(img, true);
	//QImage qImg(qImg1.size(), qImg1.format());

	QImage qImg = Image::mat2QImage(img, true);
	QPainter p(&qImg);

	for (QSharedPointer<TextLineCandidate> tlc : mTlcM) {

		for (QSharedPointer<TextComponent> tc : tlc->textComponents()) {
			
			//p.setPen(ColorManager::lightGray(0.5));
			//tc->mserBlob()->draw(p);

			if (!tc->hasRnn()) {
				//p.setPen(ColorManager::red(1.0));
				//tc->mserBlob()->draw(p);
			} 
			
			if (!tc->hasLnn()) {
				//p.setPen(ColorManager::blue(1.0));
				//tc->mserBlob()->draw(p);
			}
			
			if (tc->isAFork()) {
				//p.setPen(ColorManager::darkGray(1.0));			
				//tc->mserBlob()->draw(p);

				//for (auto b : tc->lnn()) {
				//	p.setPen(ColorManager::randColor());
				//	//b->mserBlob()->draw(p);
				//	b->drawLnns(p);
				//}
			}

			//if (tc->hasRnn()) {
			//	p.setPen(ColorManager::red(1.0));
			//	QPoint point1 = tc->mserBlob()->center().toQPoint();
			//	QPoint point2 = tc->rnn()->mserBlob()->center().toQPoint();
			//	p.drawLine(point1, point2);
			//}
		}

		for (auto ws : tlc->whiteSpaces()) {

			if (ws->isBCR()) {
				//p.setPen(ColorManager::red(1.0));
				//p.drawRect(ws->bbox().toQRect());
			}
			else {
				//p.setPen(ColorManager::red(0.5));
				//p.drawRect(ws->bbox().toQRect());
			}
		}

		//p.setPen(ColorManager::blue(1.0));
		//p.setPen(ColorManager::lightGray(1.0));
		//p.drawRect(tlc->bbox().toQRect());
	}

	for (QSharedPointer<TextLineCandidate> tlc : mTlcM) {
		p.setPen(ColorManager::green(1.0));
		p.drawRect(tlc->bbox().toQRect());
		
		QString maxGap = QString::number(tlc->maxGap());
		p.drawText(tlc->bbox().toQRect(), Qt::AlignCenter, maxGap);

		//for (auto tc : tlc->textComponents()) {
		//	p.setPen(ColorManager::lightGray(1.0));
		//	p.drawRect(tc->mserBlob()->bbox().toQRect());
		//}

		//for (auto ws : tlc->whiteSpaces()) {
		//	p.setPen(ColorManager::white(1.0));
		//	p.drawRect(ws->bbox().toQRect());
		//}
	}

	for (auto wsr : mWsrM) {
		
		for (auto ws : wsr->whiteSpaces()) {
			if (ws->isBCR()) {
				p.setPen(ColorManager::blue(1.0));
				p.drawRect(ws->bbox().toQRect());
			}
			else {
				p.setPen(ColorManager::red(1.0));
				p.drawRect(ws->bbox().toQRect());
			}

			QString gap = QString::number(ws->bbox().width());
			p.drawText(ws->bbox().bottomLeft().toQPoint(), gap);	
		}
	}

	mRnnImg = Image::qImage2Mat(qImg);

	//mRnnImg = sp.draw(img);
	//if (img.rows > 2000) {
	//	// if you stumble upon this line:
	//	// microfilm images are binary with heavy noise
	//	// a small median filter fixes this issue...
	//	cv::medianBlur(img, img, 3);
	//}

	mInfo << "computed in" << dt;

	return true;
}

QSharedPointer<WhiteSpaceAnalysisConfig> WhiteSpaceAnalysis::config() const {
	return qSharedPointerDynamicCast<WhiteSpaceAnalysisConfig>(mConfig);
}

SuperPixel WhiteSpaceAnalysis::computeSuperPixels(const cv::Mat & img){

	// compute super pixels
	SuperPixel sp = SuperPixel(img);
	if (!sp.compute()) {
		qDebug() << "error during SuperPixel computation";
	}

	return sp;
}

void WhiteSpaceAnalysis::extractTextComponents(){

	for (QSharedPointer<MserBlob> b : mBlobs) {
		QSharedPointer<TextComponent> tc = QSharedPointer<TextComponent>(new TextComponent(b));
		mTcM.append(tc);
	}

	//TODO use additional filtering to reduce amount of noise components
	//save a list of final tc used for further processing

	//sort tc according to y_max coordinate for more efficient computation
	//auto tcM_y = QVector<QSharedPointer<TextComponent>>(tcM);

	std::sort(mTcM.begin(), mTcM.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->mserBlob()->bbox().top() < rhs->mserBlob()->bbox().top();
	});

	//find right nearest neighbors (rnn) for each text component
	for (QSharedPointer<TextComponent> tc : mTcM) {
		QSharedPointer<TextComponent> rnn;
		bool foundRnn = false;

		for (QSharedPointer<TextComponent> tcn : mTcM) {

			QSharedPointer<MserBlob> tcBlob = tc->mserBlob();
			QSharedPointer<MserBlob> tcnBlob = tcn->mserBlob();

			if (tcBlob != tcnBlob) {

				Rect tcR = tcBlob->bbox();
				Rect tcnR = tcnBlob->bbox();

				//qDebug() << "tcnR is: " << tcnR.toString();
				Vector2D irSize(mImg.size().width - tcR.width(), tcR.height());
				Rect ir = Rect(tcR.topRight(), irSize);

				if (tcnR.center().x()>tcR.right() && ir.intersects(tcnR)) {	//center of neighbor should not be within components rect
					if (foundRnn) {
						if (tcnR.left() < rnn->mserBlob()->bbox().left()) {
							rnn = tcn;
						}
					}
					else {
						rnn = tcn;
						foundRnn = true;
					}
				}
				else if (tcnR.top()>tcR.bottom()) {
					break;
				}
			}
		}

		// if found  set rnn for current tc
		if (!rnn.isNull()) {
			tc->setRnn(rnn);
			rnn->setLnn(tc);

			if (rnn->lnn().size() > 1) {
				rnn->setForkMarker(true);
			}
		}
	}
}

void WhiteSpaceAnalysis::computeInitialTLC(){

	//create initial text line candidates
	for (auto tc : mTcM) {
		if (!tc->hasLnn()) {
			//create text line candidates
			QSharedPointer<TextLineCandidate> tlc;
			tlc = QSharedPointer<TextLineCandidate>(new TextLineCandidate(tc));
			mTlcM.append(tlc);
		}
	}

	//sort text lines according to y coordinate
	std::sort(mTlcM.begin(), mTlcM.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().bottom() < rhs->bbox().bottom();
	});

	refineTLC();
}

void WhiteSpaceAnalysis::findBCR(){

	//remove isolated white space rectangles
	//TODO find more efficient way of comparing locations of white spaces
	for (int i = 0; i < mTlcM.length(); ++i) {
		for (auto ws1 : mTlcM.at(i)->whiteSpaces()) {
			if (ws1->isBCR()) {
				bool topIsIsolated = true;
				bool bottomIsIsolated = true;
				Rect ws1B = ws1->bbox();

				//look for white spaces below the current one
				if (i + 1 < mTlcM.length()) {
					for (int j = i + 1; j < mTlcM.length(); ++j) {
						Rect tlcB = mTlcM.at(j)->bbox();
						//check if x span of text line covers current white space
						if (tlcB.left() < ws1B.left() && tlcB.right() > ws1B.right()) {

							for (auto ws2 : mTlcM.at(j)->whiteSpaces()) {
								if (ws2->isBCR()) {
									Rect ws2B = ws2->bbox();
									Rect ir(ws1B.left(), ws2B.top(), ws1B.width(), 1);
									if (ws2B.intersects(ir)) {
										ws2->setHasANN(true);
										ws1->addBnn(ws2);
										bottomIsIsolated = false;
										//break; //look at white spaces below in order to determine all bnn
									}
								}
							}
							break;	//only look at first text line below current ws
						}
					}
				}

				//look for white spaces above the current one
				if (i - 1 >= 0) {
					for (int j = i - 1; j >= 0; --j) {
						Rect tlcB = mTlcM.at(j)->bbox();
						//check if x span of text line covers current white space
						if (tlcB.left() < ws1B.left() && tlcB.right() > ws1B.right()) {

							for (auto ws2 : mTlcM.at(j)->whiteSpaces()) {
								if (ws2->isBCR()) {
									Rect ws2B = ws2->bbox();
									Rect ir(ws1B.left(), ws2B.top(), ws1B.width(), 1);
									if (ws2B.intersects(ir)) {
										topIsIsolated = false;
										break;
									}
								}
							}
							break;	//only look at first text line above current ws
						}
					}
				}

				if (topIsIsolated && bottomIsIsolated) {
					ws1->setIsBCR(false);
				}
			}
		}
	}
}

void WhiteSpaceAnalysis::splitTLC(){

	//TODO refactor this process for efficiency
	//TODO order of mTlcM should be kept 
	//TODO check validity of split tlc (location, size, etc.)
	QVector<QSharedPointer<TextLineCandidate>> tlcM_tmp;

	//split text lines according to bcr and compute maxGaps
	for (auto tlc : mTlcM) {

		//add text lines without white spaces
		if (tlc->whiteSpaces().length() == 0) {
			tlcM_tmp.append(tlc);
			continue;
		}

		auto tlc_ws = tlc->whiteSpaces();
		auto tlc_tc = tlc->textComponents();
		double maxGap = 0;
		int previousWS_idx = -1;
		int lastTC_idx = -1;

		for (int i = 0; i < tlc_ws.length(); ++i) {
			auto ws = tlc_ws.at(i);

			if (ws->isBCR()) {
				mWsM.append(ws);

				int firstTC_idx = lastTC_idx + 1;

				//find tc located left from current ws
				for (int j = firstTC_idx; j < tlc_tc.length(); ++j) {
					double rB = tlc_tc.at(j)->mserBlob()->bbox().right();

					if (rB == ws->bbox().left()) {
						lastTC_idx = j;
						break;
					}
				}

				//create new tlc and set parameter
				QSharedPointer<TextLineCandidate> tlc_tmp(new TextLineCandidate());
				int tc_rl = lastTC_idx - firstTC_idx + 1;
				tlc_tmp->setTextComponents(tlc_tc.mid(firstTC_idx, tc_rl));

				if (previousWS_idx + 1 <= i - 1) {
					int ws_rl = i - (previousWS_idx + 1);
					tlc_tmp->setWhiteSpaces(tlc_ws.mid(previousWS_idx + 1, ws_rl));
				}
				else {
					////qWarning() << "Trying to split tlc but resulting tlc has no ws!";
					tlc_tmp->setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>>());
					//TODO create white space vector object in tlc constructor and skip this step
				}

				tlc_tmp->setMaxGap(maxGap);
				tlcM_tmp.append(tlc_tmp);
				
				maxGap = 0;
				previousWS_idx = i;
			}
			else {
				//update maxGap
				double gap = ws->bbox().width();
				if (gap > maxGap)
					maxGap = gap;
			}

			//add last text line
			if (i == tlc_ws.length() - 1) {

				QSharedPointer<TextLineCandidate> tlc_tmp(new TextLineCandidate());
				int tc_rl = tlc_tc.length() - (lastTC_idx + 1);
				tlc_tmp->setTextComponents(tlc_tc.mid(lastTC_idx + 1, tc_rl));

				if (previousWS_idx + 1 <= i) {
					int ws_rl = tlc_ws.length() - (previousWS_idx + 1);
					tlc_tmp->setWhiteSpaces(tlc_ws.mid(previousWS_idx + 1, ws_rl));
				}
				else {
					//qWarning() << "Trying to split tlc but resulting tlc has no ws!";
					tlc_tmp->setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>>());
					//TODO create white space vector object in tlc constructor and skip this step
				}

				tlc_tmp->setMaxGap(maxGap);
				tlcM_tmp.append(tlc_tmp);
			}
		}
	}

	mTlcM = tlcM_tmp;
}

void WhiteSpaceAnalysis::groupWS() {

	//find white space runs (wsr)
	for (auto ws : mWsM) {

		if (!ws->hasANN() || ws->bnn().length() > 1) {
			for (auto bws : ws->bnn()) {
				auto wsr = QSharedPointer<WhiteSpaceRun>(new WhiteSpaceRun(ws));
				wsr->appendAllBnn(bws);
				mWsrM.append(wsr);
			}
		}
	}
}

void WhiteSpaceAnalysis::updateBCR() {

	//refine list of wsr runs
	for (auto ws : mWsM) {
		Rect wsB = ws->bbox();
		double mg = 0;

		for (auto tlc : mTlcM) {
			Rect tlcB = tlc->bbox();

			//check if x span of text line covers current white space
			if (wsB.top() >= tlcB.top() && wsB.bottom() <= tlcB.bottom()) {

				//TODO avoid iterating over all tlcs
				//TODO check if both tlc on left and right side have only one word -> setIsBCR(false)
				if (wsB.left() == tlcB.right() || wsB.right() == tlcB.left()) {
					if (tlc->maxGap() > mg)
						mg = tlc->maxGap();

					if (mg >= wsB.width()) {
						ws->setIsBCR(false);
						break;
					}
				}
			}
		}
	}
}

bool WhiteSpaceAnalysis::updateSegmentation() {

	bool updated = false;
	auto wsrM = mWsrM;
	
	for (auto wsr : wsrM) {

		int bcr_count = 0;
		auto wsM = wsr->whiteSpaces();
		for (auto ws : wsM) {
			if (ws->isBCR())
				++bcr_count;
		}

		//delete wsr
		if (bcr_count == 0 || wsM.length()==1) {
			
			//DEBUG 
			int idx = mWsrM.indexOf(wsr);
			if (idx == -1)
				int test = 1;

			if (deleteWSR(wsr)) {
				updated = true;
				continue;
			}		
		}

		if (trimWSR(wsr))
			updated = true;
	}

	return updated;
}

bool WhiteSpaceAnalysis::deleteWS(QSharedPointer<WhiteSpace> ws) {
	
	//find tlc neighboring ws and merge them
	int tlcIdx1 = -1;
	int tlcIdx2 = -1;
	for (int j = 0; j < mTlcM.length(); j++) {
		Rect wsB = ws->bbox();
		Rect tlcB = mTlcM.at(j)->bbox();
		if (wsB.top() >= tlcB.top() && wsB.bottom() <= tlcB.bottom()) {
			//if (mTlcM.at(j)->bbox().intersects(ws->bbox())) {
			if (wsB.left() == tlcB.right()) {
				tlcIdx1 = j;
			}
			if (wsB.right() == tlcB.left()) {
				tlcIdx2 = j;
			}
		}
	}

	if (tlcIdx1 == -1 || tlcIdx2 == -1) {
		//TODO check why this happens
		qWarning() << "White space analysis: Failed to delete WS!";
		return false;
	}

	auto tlc1 = mTlcM.at(tlcIdx1);
	auto tlc2 = mTlcM.at(tlcIdx2);

	if (!tlc1->merge(tlc2, ws))
		return false;

	if (tlc1->bbox().bottom() > tlc1->bbox().bottom()) {
		mTlcM.replace(tlcIdx1, tlc1);
		mTlcM.remove(tlcIdx2);
	}
	else {
		mTlcM.replace(tlcIdx2, tlc1);
		mTlcM.remove(tlcIdx1);
	}

	mWsM.removeAt(mWsM.indexOf(ws));	//remove ws

	return true;
}

bool WhiteSpaceAnalysis::deleteWSR(QSharedPointer<WhiteSpaceRun> wsr) {
	
	auto wsM = wsr->whiteSpaces();
	bool updated = false;

	for (int i = 0; i < wsM.length(); i++) {
		auto ws = wsM.at(i);

		if (mWsM.indexOf(wsM.at(i)) == -1) {
			continue; // nothin to do, ws already deleted
		}

		//remove ws in wsr with one or less bnn and merge tlc for each removed ws
		if (ws->bnn().length() <= 1) {
			if (ws->bnn().length() == 1 && (i+1) > wsM.length()) {
				continue;	//ws is still contained in another wsr
			}

			if (deleteWS(ws))
				updated = true;
		}

		//if there ís more than one bnn remove the following ws from the bnn list
		//the current ws should then be deleted if all except for one bnn have been removed
		if (ws->bnn().length() > 1) {
			if (wsM.length() > i + 1) {
				//TODO check if this works
				qInfo() << "bnn list length = " << ws->bnn().length() << " before removing";
				ws->bnn().removeAt(ws->bnn().indexOf(wsM.at(i + 1)));
				qInfo() << "bnn list length = " << ws->bnn().length() << " after removing";
			}
		}
	}

	//updated = true;
	mWsrM.removeAt(mWsrM.indexOf(wsr));	//remove wsr

	return updated;
}

bool WhiteSpaceAnalysis::trimWSR(QSharedPointer<WhiteSpaceRun> wsr){

	auto wsM = wsr->whiteSpaces();
	bool updated = false;

	//use this to shorten the wsr
	if (wsM.size() > 1 && !wsM.last()->isBCR()) {

		auto bnn = wsM.at(wsM.length() - 2)->bnn();
		bnn.removeAll(wsM.last());
		wsM.at(wsM.length() - 2)->setBnn(bnn);

		if (mWsM.indexOf(wsM.last()) != -1) {
			deleteWS(wsM.last()); //TODO nothin to do, ws already deleted -> check why this is happening
		}
		//deleteWS(wsM.last());

		wsM.removeAt(wsM.length() - 1);
		updated = true;
	}

	if (wsM.size() > 1 && !wsM.at(0)->isBCR()) {
		wsM.at(1)->setHasANN(false);
		
		if (mWsM.indexOf(wsM.at(0)) != -1) {
			deleteWS(wsM.at(0)); // nothin to do, ws already deleted
		}
		//deleteWS(wsM.at(0));

		wsM.removeAt(0);
		updated = true;
	}

	if (updated) {
		wsr->setWhiteSpaces(wsM);
	}	

	return updated;
}

void WhiteSpaceAnalysis::convertTLC() {
	//convert tlc to text regions
	for (auto tlc : mTlcM) {
		QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());
		textRegion->setPolygon(rdf::Polygon::fromRect(tlc->bbox()));
		textRegion->setId(textRegion->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors
		textRegion->setType(rdf::Region::type_text_line);

		mTextLines.append(textRegion);
	}
}

void WhiteSpaceAnalysis::refineTLC() {
	
	//remove nested/overlapping text line  candidates
	//merge text lines with large y overlap that are very close to each other
	QVector<QSharedPointer<TextLineCandidate>> tlcM_final;
	for (auto tlc1 : mTlcM) {
		bool final = true;

		for (auto tlc2 : mTlcM) {
			if (tlc1 != tlc2) {
				Rect b1 = tlc1->bbox();
				Rect b2 = tlc2->bbox();

				//merge nested or overlapping tlc
				if (b1.area() < b2.area()) {
					if (b1.intersects(b2)) {
						double overlapArea = (b1.intersected(b2)).area();
						if ((overlapArea / b1.area()) > double(0.75)) {
							final = false;
							break;
						}
					}

					double xOverlap = std::min(b1.right(), b2.right()) - std::max(b1.left(), b2.left());
					double yOverlap = std::min(b1.bottom(), b2.bottom()) - std::max(b1.top(), b2.top());
					if (xOverlap > -5 && yOverlap > 0) {
						double relYoverlap = yOverlap / std::min(b1.height(), b2.height());
						double relHeight = b1.height() / b2.height();
						if (relYoverlap > 0.75 && relHeight < 1.5) {
							final = false;
							tlc2->merge(tlc1);
							//mTlcM.replace(mTlcM.indexOf(tlc2), merge_tlc);
							break;
						}
					}
				}
			}
		}

		if (tlc1->textComponents().length() < 5) {
			continue;
		}
		if (final) {
			tlcM_final.append(tlc1);
		}
	}

	mTlcM = tlcM_final;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textLines(){
	return mTextLines;
}

QVector<QSharedPointer<TextRegion>> WhiteSpaceAnalysis::textBlocks() {
	return mTextBlocks;
}

cv::Mat WhiteSpaceAnalysis::draw(const cv::Mat & img, const QColor& col) const {

	// draw mser blobs
	Timer dtf;
	QImage qImg = Image::mat2QImage(img, true);
	QPainter p(&qImg);
	p.setPen(col);

	for (auto b : mBlobs) {

		if (!col.isValid())
			p.setPen(ColorManager::randColor());

		b->draw(p);
	}

	qDebug() << "drawing takes" << dtf;

	return Image::qImage2Mat(qImg);

	/**
	for (auto l : mStopLines) {
		l.setThickness(3);
		l.draw(p);
	}

	for (auto tb : mTextBlockSet.textBlocks()) {

		QPen pen(col);
		pen.setCosmetic(true);
		p.setPen(pen);

		QVector<PixelSet> s = tb->pixelSet().splitScales();
		for (int idx = s.size() - 1; idx >= 0; idx--) {

			if (!col.isValid())
				p.setPen(ColorManager::randColor());
			p.setOpacity(0.5);
			s[idx].draw(p, PixelSet::DrawFlags() | PixelSet::draw_pixels, Pixel::DrawFlags() | Pixel::draw_stats | Pixel::draw_ellipse);
			//qDebug() << "scale" << idx << ":" << *s[idx];
		}

		p.setOpacity(1.0);

		QPen tp(ColorManager::pink());
		tp.setWidth(5);
		p.setPen(tp);

		tb->draw(p, TextBlock::draw_text_lines);
	}
	**/
}

QString WhiteSpaceAnalysis::toString() const {
	return Module::toString();
}

bool WhiteSpaceAnalysis::checkInput() const {
	return !isEmpty();
}

// TextComponent --------------------------------------------------------------------
TextComponent::TextComponent() {
	mMserBlob = QSharedPointer<MserBlob>();
	mHasRnn = false;
	mHasLnn = false;
	mIsAFork = false;
}

TextComponent::TextComponent(QSharedPointer<MserBlob> blob){
	mMserBlob = blob;
	mHasRnn = false;
	mHasLnn = false;
	mIsAFork = false;
}

void TextComponent::setRnn(QSharedPointer<TextComponent> rn) {
	mRnn = rn;
	mHasRnn = true;
}

void TextComponent::setLnn(QSharedPointer<TextComponent> ln){
	mLnn.append(ln);
	mHasLnn = true;
}

void TextComponent::setForkMarker(bool isAFork){
	mIsAFork = isAFork;
}

QSharedPointer<TextComponent> TextComponent::rnn() const{
	return mRnn;
}

QVector<QSharedPointer<TextComponent>> TextComponent::lnn() const{
	return mLnn;
}

int TextComponent::bestFitLnnIdx() const {
	
	if (mHasLnn) {

		if (!mIsAFork) {
			return 0; //index of first (only) element
		}
		else {
			int maxRl = -1;
			int maxRlIdx = -1;
			for (int idx = 0; idx < mLnn.size(); ++idx) {
				int rl = mLnn.at(idx)->lnnRunLength();
				if (rl > maxRl) {
					maxRl = rl;
					maxRlIdx = idx;
				}
			}
			if (maxRl == -1) {
				qWarning() << "Couldn't find bestFitLnn Index!";
			}
			return maxRlIdx;
		}
	}
	else {
		return -1;
	}
}

int TextComponent::lnnRunLength() const{

	int lnnNum = 1;

	if (hasLnn()) {
		lnnNum += 1;
		
		int maxLnnRL = -1;
		for(auto tc:lnn()){
			int lnnRL = tc->lnnRunLength();
			if (lnnRL > maxLnnRL) {
				maxLnnRL = lnnRL;
			}
		}
		lnnNum += maxLnnRL;
	}

	return lnnNum;
}

void TextComponent::draw(const cv::Mat & img, const QColor& col) const {

	QImage qImg = Image::mat2QImage(img, true);

	QPainter p(&qImg);
	p.setPen(ColorManager::blue());
}

void TextComponent::drawLnns(QPainter &p){
	
	QColor col = p.pen().color();

	int lnnNum = 1;
	int maxLnnRL = -1;
	QSharedPointer<TextComponent> best_lnn;

	if (mHasLnn) {
		for (auto b : mLnn) {

			int lnnRL = b->lnnRunLength();
			if (lnnRL > maxLnnRL) {
				maxLnnRL = lnnRL;
				best_lnn = b;
			}
		}
		
		p.setPen(ColorManager::red(1.0));
		QPoint point1 = mserBlob()->center().toQPoint();
		QPoint point2 = best_lnn->mserBlob()->center().toQPoint();
		p.drawLine(point1, point2);

		col.setAlpha(60);
		p.setPen(col);

		best_lnn->mMserBlob->draw(p);
		best_lnn->drawLnns(p);

		lnnNum += maxLnnRL;
	}
}

QSharedPointer<MserBlob> TextComponent::mserBlob() const{
	return mMserBlob;
}

bool TextComponent::hasRnn() const{
	return mHasRnn;
}

bool TextComponent::hasLnn() const{
	return mHasLnn;
}

bool TextComponent::isAFork() const{
	return mIsAFork;
}

// TextLineCandidate --------------------------------------------------------------------

TextLineCandidate::TextLineCandidate(){
}

TextLineCandidate::TextLineCandidate(QSharedPointer<TextComponent> tc){
	mTextComponents.append(tc);

	if (tc->hasRnn()) {
		appendAllRnn(tc);
	}
}

bool TextLineCandidate::merge(QSharedPointer<TextLineCandidate> tlc, QSharedPointer<WhiteSpace> ws){

	//TODO if needed to exchange tlc try handing over this and call merge again
	
	double ws_width = 0;
	if (!ws.isNull()) {
		ws_width = ws->bbox().width();
	}
	
	if(bbox().left() < tlc->bbox().left()){
		
		mTextComponents.append(tlc->textComponents());
		if (!ws.isNull())
			mWhiteSpaces.append(ws);
		mWhiteSpaces.append(tlc->whiteSpaces());
		mMaxGap = std::max(mMaxGap, tlc->maxGap());
		mMaxGap = std::max(mMaxGap, std::max(ws_width, tlc->maxGap()));
	}
	else {
		if (tlc->bbox().left() < bbox().left()) {
			auto tc_tmp = tlc->textComponents();
			tc_tmp.append(mTextComponents);
			mTextComponents = tc_tmp;

			auto ws_tmp = tlc->whiteSpaces();
			if (!ws.isNull())
				ws_tmp.append(ws);
			ws_tmp.append(mWhiteSpaces);
			mWhiteSpaces = ws_tmp;

			mMaxGap = std::max(mMaxGap, std::max(ws_width, tlc->maxGap()));
		}
		else {
			qWarning() << "White space analysis: Failed to merge tlc because they seem to be overlapping";
			return false;
		}
	}
	
	return true;
}

int TextLineCandidate::length() const{
	return mTextComponents.size();
}

Rect TextLineCandidate::bbox() const {

	Rect bbox;

	for (QSharedPointer<TextComponent> tc : mTextComponents) {
		if (!bbox.isNull()) {
			bbox = bbox.joined(tc->mserBlob()->bbox());
		}
		else {
			bbox = tc->mserBlob()->bbox();
		}
	}

	return bbox;
}

void TextLineCandidate::setTextComponents(QVector<QSharedPointer<TextComponent>> textComponents){
	mTextComponents = textComponents;
}

void TextLineCandidate::setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>> whiteSpaces){
	mWhiteSpaces = whiteSpaces;
}

void TextLineCandidate::setMaxGap(double maxGap){
	mMaxGap = maxGap;
}

double rdf::TextLineCandidate::maxGap() const{
	return mMaxGap;
}

QVector<QSharedPointer<TextComponent>> TextLineCandidate::textComponents() const{
	return mTextComponents;
}

QVector<QSharedPointer<WhiteSpace>> TextLineCandidate::whiteSpaces() const{
	return mWhiteSpaces;
}

void TextLineCandidate::computeWhiteSpaces(int pageWidth){

	for (int i = 1; i < mTextComponents.length(); ++i) {
		Rect r1 = mTextComponents.at(i - 1)->mserBlob()->bbox();
		Rect r2 = mTextComponents.at(i)->mserBlob()->bbox();
		
		if (r2.left() - r1.right() > 0) {

			double wsrTop = std::max(r1.top(), r2.top());
			double wsrBottom = std::min(r1.bottom(), r2.bottom());
			Vector2D wsrTL = Vector2D(r1.right(), wsrTop);

			Vector2D wsrSize = Vector2D(r2.left() - r1.right(), wsrBottom - wsrTop);

			Rect wsr(wsrTL, wsrSize);
			QSharedPointer<WhiteSpace> ws(new WhiteSpace(wsr));
			mWhiteSpaces.append(ws);

		}
	}

	//sort white spaces according to size
	QVector<QSharedPointer<WhiteSpace>> ws_l(mWhiteSpaces);

	std::sort(ws_l.begin(), ws_l.end(), [](const auto& lhs, const auto& rhs) {
		return lhs->bbox().width() > rhs->bbox().width();
	});

	//compute max number of between column rectangles and mark them
	//TODO compute this parameter in the text line creation
	double tlcWdith = bbox().size().length();
	double bcrn = std::max(1.0, std::round(8 * tlcWdith / pageWidth));

	if (ws_l.length() < bcrn)
		bcrn = ws_l.size();

	for (int i = 0; i < bcrn; ++i) {
		ws_l.at(i)->setIsBCR(true);
	}
}

void TextLineCandidate::appendAllRnn(const QSharedPointer<TextComponent> tc){

	QSharedPointer<TextComponent> next = tc->rnn();

	if(next->hasLnn()){
		if (!next->isAFork()) {
			mTextComponents.append(next);

			if (next->hasRnn()) {
				appendAllRnn(next);
			}
		}
		else if(next->isAFork()) {

			if (next->bestFitLnnIdx() != -1) {
				QSharedPointer<TextComponent> bestFitLnn = next->lnn().at(next->bestFitLnnIdx());
				if (tc->mserBlob() == bestFitLnn->mserBlob()) {
					mTextComponents.append(next);
					if (next->hasRnn()) {
						appendAllRnn(next);
					}
				}
			} else {
				qWarning() << "Found a fork without bestFitLnn!";
			}
		}
	}	
	else {
		qWarning() << "Something went wrong! Found rnn text component without lnn.";
	}

}

// WhiteSpace --------------------------------------------------------------------
WhiteSpace::WhiteSpace(){
	mBbox = Rect();
	mIsBCR = false;
	mIsWCC = false;
	mHasANN = false;
}

WhiteSpace::WhiteSpace(Rect r){
	mBbox = r;
	mIsBCR = false;
	mIsWCC = false;
	mHasANN = false;
}

Rect WhiteSpace::bbox() const{
	return mBbox;
}

bool WhiteSpace::isNull() const{
	return mBbox.isNull();
}

void WhiteSpace::setIsBCR(bool isBCR){
	mIsBCR = isBCR;
}

void WhiteSpace::setIsWCC(bool isWCC){
	mIsWCC = isWCC;
}

bool WhiteSpace::isBCR() const{
	return mIsBCR;
}

bool WhiteSpace::isWCC() const{
	return mIsWCC;
}

bool WhiteSpace::hasANN() const
{
	return mHasANN;
}

void WhiteSpace::setHasANN(bool HasANN){
	mHasANN = HasANN;
}

QVector<QSharedPointer<WhiteSpace>> WhiteSpace::bnn(){
	return mBnn;
}

void WhiteSpace::addBnn(const QSharedPointer<WhiteSpace> ws){
	mBnn.append(ws);
}

void WhiteSpace::setBnn(const QVector<QSharedPointer<WhiteSpace>> bnn){
	mBnn = bnn;
}

WhiteSpaceRun::WhiteSpaceRun(){

}

WhiteSpaceRun::WhiteSpaceRun(QSharedPointer<WhiteSpace> ws){
	mWhiteSpaces.append(ws);
}

void WhiteSpaceRun::appendAllBnn(const QSharedPointer<WhiteSpace> ws){
	
	//TODO append bnn only if it shares a common x span with the previous components

	if (!ws->bnn().isEmpty()) {

		if (ws->bnn().length() == 1) {
			mWhiteSpaces.append(ws);
			appendAllBnn(ws->bnn().at(0));
		}
		else {
			if (ws->bnn().length() > 1)
				return;
		}
	}
	else {
		mWhiteSpaces.append(ws);
	}
}

QVector<QSharedPointer<WhiteSpace>> WhiteSpaceRun::whiteSpaces(){
	return mWhiteSpaces;
}

void WhiteSpaceRun::setWhiteSpaces(QVector<QSharedPointer<WhiteSpace>> whiteSpaces){
	mWhiteSpaces = whiteSpaces;
}

// TextBlockFormation --------------------------------------------------------------------
TextBlockFormation::TextBlockFormation(){
}

TextBlockFormation::TextBlockFormation(QVector<QSharedPointer<TextRegion>> textLines){
	mTextLines = textLines;
}

bool TextBlockFormation::compute(){

	if (mTextLines.isEmpty())
		return false;
	
	computeAdjacency();
	formTextBlocks();

	return true;
}

void TextBlockFormation::computeAdjacency(){

	//Compute below and above nearest neighbors for tlc
	bnnIndices = QVector<QVector<int>>(mTextLines.length());
	annCount = QVector<int>(mTextLines.length(), 0);

	for (int i = 0; i < mTextLines.length(); ++i) {
		Rect r1 = Rect::fromPoints(mTextLines.at(i)->polygon().toPoints());
		Rect bnn1;

		for (int j = i + 1; j < mTextLines.length(); ++j) {
			Rect r2 = Rect::fromPoints(mTextLines.at(j)->polygon().toPoints());

			//check for x overlap of tlc
			if (r1.left() <= r2.right() && r2.left() <= r1.right()) {
				if (bnn1.isNull()) {
					bnnIndices[i] = QVector<int>(1, j);
					annCount[j] = annCount[j] + 1;
					bnn1 = r2;
				}
				else {
					//check for y overlap with previously found bnn
					double overlap = std::min(bnn1.bottom(), r2.bottom()) - std::max(bnn1.top(), r2.top());
					double relOverlap = overlap / std::min(bnn1.height(), r2.height());
					if (overlap > 0 && relOverlap > 0.5) {
						//if (bnn1.top() <= r2.bottom() && r2.top() <= bnn1.bottom()) {
						bnnIndices[i] << j;
						annCount[j] = annCount[j] + 1;
					}
					else {
						break;
					}
				}
			}
		}
	}
}

void TextBlockFormation::formTextBlocks(){

	//form text blocks by grouping text (line) regions
	for (int i = 0; i < mTextLines.length(); ++i) {

		if (annCount.at(i) != 1) {
			QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());

			textRegion->addChild(mTextLines[i]);

			if (bnnIndices.at(i).length() == 1) {
				appendTextLines(i, textRegion);
			}

			mTextBlocks.append(textRegion);
		}

		if (bnnIndices.at(i).length() > 1) {
			for (int idx : bnnIndices.at(i)) {
				if (annCount[idx] == 1) {
					QSharedPointer<rdf::TextRegion> textRegion(new rdf::TextRegion());
					textRegion->addChild(mTextLines[idx]);

					if (bnnIndices.at(idx).length() == 1) {
						appendTextLines(idx, textRegion);
					}

					mTextBlocks.append(textRegion);
				}
			}
		}
	}

	// compute bbox of text blocks and set region type
	for (auto tb : mTextBlocks) {
		Rect bb;
		for (auto c : tb->children()) {
			if (bb.isNull()) {
				bb = Rect::fromPoints(c->polygon().toPoints());
			}
			else {
				Rect cbb = Rect::fromPoints(c->polygon().toPoints());
				bb = bb.joined(cbb);
			}
		}

		tb->setPolygon(rdf::Polygon::fromRect(bb));
		tb->setId(tb->id().remove("{").remove("}"));	// remove parentheses to please Aletheia and avoid errors
		tb->setType(rdf::Region::type_text_region);
	}
}

void TextBlockFormation::appendTextLines(int idx, QSharedPointer<rdf::TextRegion> textRegion) {

	int nextIdx = bnnIndices.at(idx)[0];
	while (true) {

		if ((annCount.at(nextIdx) > 1)) {
			break;
		}

		textRegion->addChild(mTextLines[nextIdx]);
		if (bnnIndices.at(nextIdx).length() != 1) {
			break;
		}
		nextIdx = bnnIndices.at(nextIdx)[0];
	}
}

QVector<QSharedPointer<TextRegion>> TextBlockFormation::textBlocks(){
	return mTextBlocks;
}

}