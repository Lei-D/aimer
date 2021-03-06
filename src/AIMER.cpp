#include <RcppArmadillo.h>
#include <iostream>
#include "irlb.h"

// [[Rcpp::depends(RcppArmadillo)]]

arma::colvec marginalRegressionTT(arma::mat X, arma::colvec y){
    int n = y.n_elem;
    int k = X.n_cols;
    double syy = 0;
    double sxy = 0;
    double sxx = 0;
    double numer = 0;
    double std = 0;
    arma::colvec output = arma::zeros<arma::colvec>(k);
    for(int j = 0; j < k; j++){
        sxx = 0;
        sxy = 0;
        for(int i = 0; i < n; i++){
            if(j == 0){
                syy = syy + y(i)*y(i);
            }
            sxy = sxy + X(i, j)*y(i);
            sxx = sxx + X(i, j)*X(i, j);
        }
        numer = sxy/sxx;
        std = sqrt(((syy/sxx) - numer*numer)/(n - 2));
        output(j) = numer/std;
    }
    return output;
}

struct TrainTest{
    arma::mat Xtrain;
    arma::mat Xtest;
    arma::colvec ytrain;
    arma::colvec ytest;
};



//returns a TrainTest (defined above) where the test data is the rows
//from start to stop including stop, and the train data is all of the rest.
TrainTest ttsplit(arma::mat X, arma::colvec y, int start, int stop){
    TrainTest output;
    output.Xtest = X.rows(start, stop);
    output.ytest = y.subvec(start, stop);
    if(start == 0){
        output.Xtrain = X.rows(stop + 1, X.n_rows - 1);
        output.ytrain = y.subvec(stop + 1, y.n_elem - 1);
    }
    else if(stop == y.n_elem - 1){
        output.Xtrain = X.rows(0, start - 1);
        output.ytrain = y.subvec(0, start - 1);
    }
    else{
        output.Xtrain = arma::join_cols(X.rows(0, start - 1),
                                        X.rows(stop + 1, X.n_rows - 1));
        output.ytrain = arma::join_cols(y.subvec(0, start-1), y.subvec(stop + 1, X.n_rows - 1));
    }
    return output;
}

arma::uvec invert(arma::uvec v){
    int val;
    arma::uvec output = arma::uvec(v.n_elem);
    for(int i = 0; i < v.n_elem; i++){
        val = v(i);
        output(val) = i;
    }
    return output;
}

// [[Rcpp::export]]
arma::colvec AIMER(arma::mat X, arma::colvec y,
                   double nCovs, double nCovsSelect, int nComps){  //FIX VARIABLES
    arma::colvec xt = arma::abs(marginalRegressionTT(X, y));
    arma::uvec indices = arma::sort_index(xt);
    arma::mat Xnew = X.cols(indices);
    arma::mat F = arma::trans(Xnew) * Xnew.cols(Xnew.n_cols - nCovs, Xnew.n_cols - 1);
    if(F.n_cols > F.n_rows){
        return arma::zeros<arma::colvec>(1);  //error
    }
    arma::mat UF = arma::zeros<arma::mat>(F.n_rows, nComps + 7);
    arma::vec SF = arma::zeros<arma::vec>(nComps + 7);
    arma::mat VF = arma::zeros<arma::mat>(nComps + 7, F.n_cols);
    VF.col(0) = arma::randn(VF.n_rows);
    if((F.n_cols < (nComps + 7)) || (nComps >= (0.5*F.n_cols))){
        arma::svd(UF, SF, VF, F);                        //full svd
    }
    else{
        int isError = irlb(F.memptr(), F.n_rows, F.n_cols, nComps, SF.memptr(), UF.memptr(), VF.memptr());   //partial svd
        if(isError != 0){
            return arma::zeros<arma::colvec>(1); //error
        }
    }
    arma::mat Vd = UF.cols(0, nComps - 1);
    arma::vec Sd = arma::sqrt(SF.subvec(0, nComps - 1));
    arma::mat VSInv = arma::zeros<arma::mat>(Vd.n_rows, Sd.n_elem);
    double sinv;
    for(int i = 0; i < VSInv.n_cols; i++){   //efficiently multiplies diagonal matrix
        sinv = 1/Sd(i);
        for(int j = 0; j < VSInv.n_rows; j++){
            VSInv(j, i) = sinv * Vd(j, i);
        }
    }
    arma::mat Ud = Xnew * VSInv;
    arma::colvec beta = VSInv * trans(Ud) * y;
    arma::uvec betaSortedIndices = invert(arma::sort_index(arma::abs(beta)));
    for(int i = 0; i < beta.n_elem; i++){
        if(betaSortedIndices(i) < (beta.n_elem - nCovsSelect)){
            beta(i) = 0;
        }
    }
    beta = beta.elem(invert(indices));
    return beta;
}








//[[Rcpp::export]]
Rcpp::List findThresholdSel(arma::mat X, arma::colvec y, arma::colvec ncomps,
                              arma::colvec nCovs,
                              int nthresh,
                              int kfold,
                              arma::colvec nCovsSelect,
                              int nthreshSelect){
    arma::field<arma::colvec> CVmse = arma::field<arma::colvec>(nthreshSelect, ncomps.n_elem, nthresh);
    int start = 0;
    int stop;
    double foldSize = ((double) X.n_rows)/((double) kfold);
    int mx = arma::max(ncomps);
    for(int k = 0; k < kfold; k++){    //for each fold
        stop = (k + 1)*foldSize - 1;
        TrainTest tt = ttsplit(X, y, start, stop);
        arma::colvec tStats = arma::abs(marginalRegressionTT(tt.Xtrain, tt.ytrain));
        arma::uvec indices = arma::sort_index(tStats);
        for(int i = 0; i < nthresh; i++){   //tries different number nCovs[i] highest marginal correlations to use
            arma::mat Xnew = tt.Xtrain.cols(indices);
            arma::mat F = arma::trans(Xnew) * Xnew.cols(Xnew.n_cols - nCovs(i), Xnew.n_cols - 1);
            if(F.n_cols > F.n_rows){
                return Rcpp::List::create(Rcpp::Named("Error") = "F has more columns than rows",
                                          Rcpp::Named("columns") = F.n_cols,
                                          Rcpp::Named("rows") = F.n_rows);
            }
            arma::mat UF = arma::zeros<arma::mat>(F.n_rows, mx + 7);
            arma::vec SF = arma::zeros<arma::vec>(mx + 7);
            arma::mat VF = arma::zeros<arma::mat>(mx + 7, F.n_cols);
            VF.col(0) = arma::randn(VF.n_rows);
            if((F.n_cols < (mx + 7)) || (mx >= (0.5*F.n_cols))){
                arma::svd_econ(UF, SF, VF, F);                        //full svd
            }
            else{
                int isError = irlb(F.memptr(), F.n_rows, F.n_cols, mx, SF.memptr(), UF.memptr(), VF.memptr());   //partial svd
                if(isError != 0){
                    return Rcpp::List::create(Rcpp::Named("Error") = isError,
                                              Rcpp::Named("ncomps") = mx,
                                              Rcpp::Named("Fcolumns") = F.n_cols);
                }
            }
            for(int j = 0; j < ncomps.n_elem; j++){   //tries different number ncomps[j] largest singular values to use
                arma::mat Vd = UF.cols(0, ncomps[j] - 1);
                arma::vec Sd = arma::sqrt(SF.subvec(0, ncomps[j] - 1));
                arma::mat VSInv = arma::zeros<arma::mat>(Vd.n_rows, Sd.n_elem);
                double sinv;
                for(int l = 0; l < VSInv.n_cols; l++){    //efficiently multiplies a diagonal matrix
                    sinv = 1/Sd(l);
                    for(int m = 0; m < VSInv.n_rows; m++){
                        VSInv(m, l) = sinv * Vd(m, l);
                    }
                }
                arma::mat Ud = Xnew * VSInv;
                arma::colvec initialBeta = VSInv * trans(Ud) * tt.ytrain;
                //Select process
                for(int l = 0; l < nthreshSelect; l++){    //tries different number nCovsSelect(l) largest betas to keep
                    arma::colvec beta = initialBeta;
                    arma::uvec sortedBetaIndices = invert(arma::sort_index(arma::abs(beta)));
                    for(int m = 0; m < beta.n_elem; m++){   //sets the remaining betas to zero
                        if(sortedBetaIndices(m) < (beta.n_elem - nCovsSelect(l))){     //abs() only worked for integers
                            beta(m) = 0;
                        }
                    }
                    //no straightforward way to boolean index, so I use the whole test matrix and beta vector with zeros.
                    arma::mat testX = tt.Xtest.cols(indices);
                    arma::colvec Yhat = testX * beta;
                    if(k == 0){
                        CVmse(l, j, i) = arma::colvec(kfold);
                    }
                    CVmse(l, j, i)(k) = arma::mean(arma::square((tt.ytest - Yhat)));
                }
            }
        }
        start = stop + 1;
    }
    arma::cube mse = arma::cube(nthreshSelect, ncomps.n_elem, nthresh);
    int bestnthresh = 0;
    int bestncomp = 0;
    int bestnthreshSelect = 0;
    arma::vec temp = CVmse(0, 0, 0);
    double minMSE = arma::mean(temp);
    for(int i = 0; i < nthreshSelect; i++){      //finds the smallest mse (and the best parameters)
        for(int j = 0; j < ncomps.n_elem; j++){
            for(int k = 0; k < nthresh; k++){
                temp = CVmse(i, j, k);
                mse(i, j, k) = arma::mean(temp);
                if(mse(i,j,k) < minMSE){
                    bestnthreshSelect = i;
                    bestncomp = j;
                    bestnthresh = k;
                    minMSE = mse(i, j, k);
                }
            }
        }
    }
    arma::colvec beta = AIMER(X, y, nCovs[bestnthresh], nCovsSelect[bestnthreshSelect], ncomps[bestncomp]);
    return Rcpp::List::create(Rcpp::Named("nCov.select.best") = nCovsSelect[bestnthreshSelect],
                              Rcpp::Named("ncomp.best") = ncomps[bestncomp],
                              Rcpp::Named("nCov.best") = nCovs[bestnthresh],
                              Rcpp::Named("ncomps") = ncomps,
                              Rcpp::Named("nCovs") = nCovs,
                              Rcpp::Named("nCovs.select") = nCovsSelect,
                              //Rcpp::Named("CVmse") = CVmse,       //Rcpp can't return this
                              Rcpp::Named("mse") = mse,
                              Rcpp::Named("beta") = beta);
}
