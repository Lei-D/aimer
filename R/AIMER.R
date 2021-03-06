#' aimer: Amplified, Initially Marginal, Eigenvector Regression
#'
#'
#' The aimer package implements the aimer algorithm as described in
#' \href{https://doi.org/10.1093/bioinformatics/btx265}{Ding and McDonald (2017)}.
#' The main goal is to use marginal regression to select a subset of coefficients, use
#' matrix approximation to estimate the principal components, and then extend those estimates
#' to the original predictor space before thresholding.
#'
#' The package uses fast C++ routines to quickly perform matrix computations and cross validation for
#' selection of tuning parameters.
#'
#' The package provides functions for estimating the model, choosing tuning parameters, and
#' generating simulated data as well as methods for prediction, plotting, and extraction.
#'
#' @section Estimation functions:
#'
#' [raimer()] Perform Amplified, Initially Marginal, Eigenvector Regression (AIMER) for fixed t, b, and d.
#'
#' @section Selecting tuning parameters:
#' [findThresholdSelect()] Find Optimal Threshold for Amplified, Initially Marginal,
#' Eigenvector Regression (AIMER) With Further Selection
#'
#' @section Simulation:
#'
#'
#' @section Methods:
#'
#'
#' @docType package
#' @name aimer
#' @md
NULL

#' @useDynLib aimer
#' @importFrom Rcpp sourceCpp
#' @importFrom Rcpp evalCpp
NULL
