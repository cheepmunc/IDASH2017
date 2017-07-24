#include "TestSGD.h"

#include <NTL/BasicThreadPool.h>
#include <NTL/ZZ.h>
#include <Params.h>
#include <PubKey.h>
#include <Scheme.h>
#include <SchemeAlgo.h>
#include <SchemeAux.h>
#include <SecKey.h>
#include <TimeUtils.h>
#include <NumUtils.h>
#include <cmath>
#include <iostream>

#include "CipherSGD.h"
#include "SGD.h"

using namespace NTL;

//-----------------------------------------

void TestSGD::testSGD(long logN, long logl, long logp, long L) {
	cout << "!!! START TEST SGD !!!" << endl;
	//-----------------------------------------
	TimeUtils timeutils;
	SetNumThreads(8);
	//-----------------------------------------
	SGD sgd;
	string filename = "data.txt";

	long dim = 0;
	long sampledim = 0;

	long** zdata = sgd.zdataFromFile(filename, dim, sampledim); //dim = 103, sampledim = 1579

	long sampledimbits = (long)ceil(log2(sampledim)); // log(1579) = 11
	long po2sampledim = (1 << sampledimbits); // 1579 -> 2048
	long learndim = (1 << (sampledimbits - 1)); // 1024
//	long slots =  (1 << (logN-1)); // N /2
	long slots =  learndim; // N /2
	long wnum = slots / learndim;
//	long learndim = sampledim;
//	long slots =  (1 << (logN-1)); // N /2
//	long wnum = slots / po2sampledim; // N / 2 / 2048
	long dimbits = (long)ceil(log2(dim)); // log(103) = 7
	long po2dim = (1 << dimbits); //103 -> 128

	cout << "dimension: " << dim << endl;
	cout << "power of 2 dimension: " << po2dim << endl;

	cout << "sample dimension: " << sampledim << endl;
	cout << "power of 2 sample dimension: " << po2sampledim << endl;

	cout << "learn dimension: " << learndim << endl;

	cout << "slots: " << slots << endl;
	cout << "wnum: " << wnum << endl;

	double** vdata = new double*[wnum];
	double** wdata = new double*[wnum];
	for (long l = 0; l < wnum; ++l) {
		wdata[l] = new double[dim];
		vdata[l] = new double[dim];
		for (long i = 0; i < dim; ++i) {
			double tmp = (1.0 - 2.0 * (double)rand() / RAND_MAX) / 64.0;
//			double tmp = 0.0;
			wdata[l][i] = tmp;
			vdata[l][i] = tmp;
		}
	}

	long iter = 20;
	long additer = 3;
	long totaliter = iter + additer;

	double* alpha = new double[iter + 2];
	alpha[0] = 0.0;
	for (long i = 1; i < iter + 2; ++i) {
		alpha[i] = (1. + sqrt(1. + 4.0 * alpha[i-1] * alpha[i-1])) / 2.0;
	}

//	double lambda = 2.0;

	timeutils.start("sgd");
	for (long k = 0; k < iter; ++k) {
//		double gamma = 5.0 / (k+1);
		double gamma = 1.0 / 5.0;
		double eta = (1. - alpha[k+1]) / alpha[k+2];
		cout << eta << endl;
		NTL_EXEC_RANGE(wnum, first, last);
		for (long l = first; l < last; ++l) {
//			sgd.stepQuadraticRegress(wdata[l], zdata, gamma[k], lambda, dim, learndim);
//			sgd.stepLogRegress(wdata[l], zdata, gamma[k], lambda, dim, learndim);
//			sgd.stepMomentumLogRegress(wdata[l], vdata[l], zdata, gamma[k], lambda, dim, learndim, eta);
			sgd.stepNesterovLogRegress(wdata[l], vdata[l], zdata, gamma, dim, learndim, eta);
		}
		NTL_EXEC_RANGE_END;
	}
	timeutils.stop("sgd");

	double* w = sgd.waverage(wdata, wnum, dim);

	sgd.check(w, zdata, dim, sampledim);

	//-----------------------------------------
	Params params(logN, logl, logp, L);
	SecKey secretKey(params);
	PubKey publicKey(params, secretKey);
	SchemeAux schemeaux(params);
	Scheme scheme(params, publicKey, schemeaux);
	SchemeAlgo algo(scheme);
	CipherSGD csgd(scheme, algo, secretKey);
	//-----------------------------------------

	timeutils.start("Enc zdata");
	Cipher* czdata = csgd.enczdata(zdata, slots, wnum, dim, learndim, params.p);
	timeutils.stop("Enc zdata");

	timeutils.start("Enc wdata");
	Cipher* cwdata = csgd.encwdata(wdata, slots, wnum, dim, learndim, params.logp);
	timeutils.stop("Enc wdata");

	//-----------------------------------------
	for (long k = iter; k < totaliter; ++k) {
		ZZ pgamma = ZZ(0);
		double lambda = 2.0;
		cout << k << endl;
		timeutils.start("Enc sgd step");
		csgd.encStepQuadraticRegress(czdata, cwdata, pgamma, lambda, slots, wnum, dim, learndim);

		timeutils.stop("Enc sgd step");
	}

	timeutils.start("Enc w out");
	Cipher* cw = csgd.encwout(cwdata, wnum, dim);
	timeutils.stop("Enc w out");

	timeutils.start("Dec w");
	double* dw = csgd.decw(secretKey, cw, dim);
	timeutils.stop("Dec w");

	sgd.check(dw, zdata, dim, sampledim);
	//-----------------------------------------
	cout << "!!! END TEST SGD !!!" << endl;
}
