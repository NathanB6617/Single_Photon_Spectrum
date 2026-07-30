#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <algorithm>
#include <tuple>

#include "TCanvas.h"
#include "TF1.h"
#include "TLegend.h"
#include "TLatex.h"
#include "TH1D.h"
#include "TPaveText.h"
#include "TStyle.h"

#include "TH1.h"
#include "TF1.h"
#include "TROOT.h"
#include "TMath.h"
#include "TSpectrum.h"
#include "TGraphErrors.h"

//this code uses the orignal code, readTxtFilesFromFolder as a basis. 

//this code is for the CAEN digitizer readings we got when we measured cosmic rays
//before running this code, please sort the readings into a preferred destination and label each folder 
//however you like. Should you use this code, please be careful as you will have to likely slightly modify 
//the names of the folders and the path to the main folder. 

//HOWEVER, main big thing is that this code uses #include <filesystem> which is not directly supported in 
//ROOT, therefore you will have to run root .L readTxtFilesFromFolder.C and not just execute it directly 
//in ROOT. You may also need a compilier like clang++ if you want to run outside of ROOT.

//to run outside of ROOT: 
//1) clang++ -std=c++17 -O2 -o FILENAME FILENAME.C `root-config --cflags --libs`
//2) ./FILENAME
//to run inside of ROOT: 
//1) root (to open ROOT) 
//2) .L FILENAME.C+ (forces ROOT to run with a compilier) 
//3) main() (calls main)


using namespace std;
namespace fs = std::filesystem;

//graphing function prototype
void Full_Graph(const vector<double>& HG_vals, const fs::directory_entry& entry, const vector<double>& channel_num, string NamedFile);

//vector function to find the peaks of the SPS (using those to find the troughs)
vector<int> FindPEPeaks(TH1D* h);

//Error graph to build the langauss from the troughs of the SPS graph
TGraphErrors* BuildTroughGraph(TH1D* h, const vector<int>& peakBins);

//Fit the troughs to a langauss
TF1* FitTroughLangau(TGraphErrors* g, double xmin, double xmax);

//function to subtract the langauss fit
TH1D* SubtractBackground(TH1D* h, TF1* bg);

//function to fit the photon peaks to Guassian functions and find the ADC to PE
void FitPhotonPeaks(TH1D* h, TGraphErrors* g);

//ROOT functions (taken from the depository)
double langaufun(double *x, double *par);
TF1 *langaufit(TH1F *his, double *fitrange, double *startvalues, double *parlimitslo, double *parlimitshi, double *fitparams, double *fiterrors, double *ChiSqr, int *NDF);
void langaus();

int main() {
  //IMPORTANT
  //this is the path to the FOLDER where the files you want to read are. Simply click on the folder in your
  //file explorer and click "copy as path" and then paste it here.

  //Keep in mind that this file path assumes that the data is stored as 1 file (1 txt that is) in a location that
  // is labeled as Run_ where the _ is a number (1, 2, 3, etc). 
  
  string basic_path = R"(/home/lfhcal/Jessie/Cosmic_Data/4_Tile_Setup/M3_M4/SPS/Run)";

  //prompt the user what file they would like
  char answer;
  string NamedFile;
  int RunNum;
  cout << "What run  would you like to read from?" << endl;
  cin >> RunNum;
  string path = basic_path + to_string(RunNum);

   //check if path exists
  if (!fs::exists(path)) {
    cerr << "Directory does not exist: " << path << endl;
    return 1;
  }
  
  cout << "Would you like to read from the specified folder? (y/n): " << path << endl;
  cin >> answer;

  cout << "Please enter a name for your graph:          ";
  cin >> NamedFile;

  if (answer == 'y' || answer == 'Y') {
    //loops over every file in the "path" which in this case is the folder, therefore
    //it loops over every file in the folder.
    for (const auto & entry : fs::directory_iterator(path)) {
	
      //this if statement just checks if it is a "regular file" and is a built in function.
      if (entry.is_regular_file()) {

	//check that the file is a .txt file
	if (entry.path().extension() != ".txt") {
	  //still state what file you are reading from, but now state that it isn't a .txt
	  cout << "Reading from file: " << entry.path() << endl;
	  cerr << "This file is not a .txt file."  << endl;
	  cout << endl << endl;
	  //continue to the next iteration
	  continue;
	}
	
	//state which folder and file you are reading from
	cout << "Reading from folder: " << entry.path().parent_path().filename() << endl;
	cout << "Reading from file: " << entry.path() << endl;
	
	//create and open the file in the folder
	ifstream inputFile;
	inputFile.open(entry.path());
	
	//make sure the file opened correctly
	if (inputFile.fail()) {
	  cerr << "Couldn't open the file!" << endl;
	  continue;
	}

	//extra check to debug if the file is empty for some reason
	if (inputFile.peek() == std::ifstream::traits_type::eof()) {
	  cerr << "File is empty or unreadable." << endl;
	  cout << endl << endl;
	  continue;
	}

	//skip the first 9 lines (they should just be the ones telling us stuff about the run)
	string line;
	for (int i = 0; i < 9; ++i) {
	  getline(inputFile, line);
	}
	
	//create vectors to store the low and high gains
	vector<double> HG_vals;
	vector<double> channel_num;

	//read in the data lines from the txt files (only the low and high gains in columns 3 and 4)
	double col[7];
	int inc=0;
	int x=0;
	//for whatever reason the txt files for this config skip columns in Tstamp_us, TrgID, and Nhits every 3 times
	//for that reason PLEASE CHECK YOUR TXT FILE to make sure the format works with this loop, if it changes please
	//alter accordingly
	while (inputFile >> x) {
	  inputFile >> col[0] >> col[1] >> col[2] >> col[3] >> col[4] >> col[5];
	  channel_num.push_back(col[0]); //Channel Number (from column 2 of txt file)
	  HG_vals.push_back(col[2]); //High Gain (from column 4 of txt file)
	  inc++;
	}

	for (int i = 0; i<9; i++) {
	  cout << HG_vals[i] << endl;
	}
	
	//close input file
	inputFile.close();
	
	//check if data was read
	int n = HG_vals.size();
	if (n == 0) {
	  cerr << "No data found after skipping first 9 lines." << endl;
	  continue;
	}
	
	//call the function to plot the graphs	  
	Full_Graph(HG_vals, entry, channel_num, NamedFile);

	//add a divide between file to make it look better in terminal
	cout << endl << endl;
      }
    }
  }

  else if (answer == 'n' || answer == 'N') {
    cout << "Okay, have a nice day! =)" << endl;
    cout << "If you would like to change the folder path, please do so in line 49 where path is defined" << endl;
  }
    return 0;
}

void Full_Graph(const vector<double>& HG_vals, const fs::directory_entry& entry, const vector<double>& channel_num, string NamedFile) {

  //this function should take the two vectors that will be made from the txt files and plot them 
  //in ROOT. 
  
  //make a variable for the size of the graph
  int n;
  n = HG_vals.size();
  
  //get the file name again so we can use it as a title
  string file = entry.path().filename().string();
  string filename = file.substr(0, file.find_last_of("."));
  
  //create canvas
  TCanvas* c1 = new TCanvas("c1", filename.c_str(), 1200, 900);
  c1->Divide(1,2);
  c1->cd(1);
  
  //CHANGE THIS FOR THE AMOUNT OF TIME OF RUN
  //create the histograms for each channel's High Gain (channel 0 and 32)
  TH1D* h1 = new TH1D("h1", "SPS Using LED Source - Background Included", 1000, 0, 1000);
  
  //set and center the x axis
  h1->SetXTitle("[ADC]");
  h1->GetXaxis()->SetTitleOffset(1.2);
  h1->GetXaxis()->CenterTitle();

  //set and center the y axis
  h1->SetYTitle("Number of Counts");
  h1->GetYaxis()->SetTitleOffset(1.2);
  h1->GetYaxis()->CenterTitle();
  h1->SetLineColor(kGreen+2);

  //fill the histogram based on the data
  for (int i=0; i<n; i++) {
      h1->Fill(HG_vals[i]);
  }

  //draw all of them on the same graph
  gStyle->SetOptStat(0);
  h1->Draw("HIST");

  //store the peaks in a variable (auto just auto assigns the correct variable bc idk what it should be)
  auto peaks = FindPEPeaks(h1);

  //check each peak location (note in the code that the first peak is almost always a pedestal
  cout << endl << "Note that the first peak is likely a pedestal" << endl;
  for(auto p : peaks)
    {
      cout << "Peak at ADC " << h1->GetBinCenter(p) << endl;
    }

  //quick error message just in case
  if(peaks.size() < 2)
    {
      cout << "Not enough peaks found!" << endl;
      return;
    }

  //send to functions to build the subtracted background
  TGraphErrors* gTroughs = BuildTroughGraph(h1, peaks);
  TF1* fBackground = FitTroughLangau(gTroughs, h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());

  fBackground->SetLineColor(kRed);
  fBackground->SetLineWidth(2);

  gTroughs->SetMarkerStyle(20);
  gTroughs->SetMarkerColor(kBlue);

  gTroughs->Draw("P SAME");
  fBackground->SetLineColor(kViolet);
  fBackground->Draw("SAME");

  //make it a histogram to plot new
  TH1D* hSub = SubtractBackground(h1, fBackground);

  //make a dummy line and triangle to add to the legend(s)
  TLine *dummyGausFit = new TLine(0,0,1,0);
  dummyGausFit->SetLineColor(kRed);
  TMarker *dummyPeaks = new TMarker(0,0,23);
  dummyPeaks->SetMarkerColor(kRed);
  
  //add a legend to the first graph
  TLegend* legend = new TLegend(0.65, 0.75, 0.85, 0.88);
  legend->SetTextSize(0.03);
  legend->AddEntry(h1, "Raw Photon Peaks", "l");
  legend->AddEntry(fBackground, "Langauss Fit on Background", "l");
  legend->AddEntry(gTroughs, "Troughs", "p");
  legend->AddEntry(dummyPeaks, "Peaks", "p");
  legend->Draw("SAME");

  //put the new graph on a second set of axes
  c1->cd(2);

  hSub->SetTitle("SPS Using LED Source - Background Subtracted");
  hSub->SetLineColor(kBlue);
  hSub->GetListOfFunctions()->Clear();
  hSub->Draw("HIST");

  //add a legend to the second graph
  TLegend* legend2 = new TLegend(0.65, 0.75, 0.85, 0.88);
  legend2->SetTextSize(0.03);
  legend2->AddEntry(hSub, "Photon Peaks", "l");
  legend2->AddEntry(dummyGausFit, "Gaussian Fits", "l");
  legend2->Draw("SAME");

  //fit the photon peaks as gaussians and calculate the gain
  FitPhotonPeaks(hSub, gTroughs);
    
  //save the file in the corresponding folder
  //IMPORTANT if reusing code you will have to change the file path to your selected folder
  //Also note that it is commented out here because it really isn't needed. 
  //c1->Print(Form("/home/lfhcal/Nathan/SPS/Graphs/%s.png", NamedFile.c_str()));
  //c1->Print(Form("/home/lfhcal/Nathan/SPS/Graphs/%s.root", NamedFile.c_str()));


  //update the graph
  c1->Update();
}

vector<int> FindPEPeaks(TH1D* h) {

  //TSpectrum appears to be out of date -> maybe change to RootFit - but it works for now
  //TSpectrum is for background estimation out of ROOT
  TSpectrum spec(8);   //how many peaks you expect to see

  TH1D *hs = (TH1D*)h->Clone("hs");
  hs->Smooth(3);
 
  //search for peaks
  int nfound = spec.Search(hs,10,"",0.05);    

  Double_t *xpeaks = spec.GetPositionX();

  vector<int> peakBins;

  //find the peaks positions
  for (int i = 0; i < nfound; i++) {
    if (xpeaks[i] > 50) {
      peakBins.push_back(h->GetXaxis()->FindBin(xpeaks[i]));
    }
  }

  //sort them to be accurate
  sort(peakBins.begin(), peakBins.end());

  return peakBins;
}

TGraphErrors* BuildTroughGraph(TH1D* h, const vector<int>& peakBins) {

  //TGraphErrors is standard for subtraction
  //goal here is to get an error graph g that can be modeled with the troughs of the peaks to be later fit
  TGraphErrors* g = new TGraphErrors();
  int point = 0;

  //search between peaks for a trough
  for(size_t i=0;i<peakBins.size()-1;i++)
    {
      int left  = peakBins[i];
      int right = peakBins[i+1];

      double minVal = 1e30;     //any very large number works I think
      int minBin = left;

      //search for the minimum value bewteen the peaks for the trough
      for(int b=left;b<=right;b++)
        {
	  double y = h->GetBinContent(b);

	  if(y < minVal)
            {
	      minVal = y;
	      minBin = b;
            }
        }

      g->SetPoint(point, h->GetBinCenter(minBin), minVal);

      g->SetPointError(point, 0, sqrt(max(minVal,1.0)));

      point++;
    }

  return g;
}


TF1* FitTroughLangau(TGraphErrors* g, double xmin, double xmax) {

  //create a new fit graph that uses the given ROOT software 
  TF1* fLangau = new TF1("fLangau", langaufun, xmin, xmax, 4);

  fLangau->SetParNames("Width","MP", "Area","GSigma");

  //fLangau->SetParameters(20, 80, 5000, 15);     // order of these is width, mp, area, sigma

  // this one bases the starting values of the fit off of the data and not hard-coded
  double x0, y0;
  g->GetPoint(0, x0, y0);

  double area = 0;
  for (int i = 0; i < g->GetN(); i++) {
    double x,y;
    g->GetPoint(i,x,y);
    area += y;
  }

  fLangau->SetParameters(
			 25,        // width
			 x0,        // MP near first trough
			 area*20,   // rough normalization
			 15
			 );
  
  g->Fit(fLangau,"QR");

  return fLangau;
}


TH1D* SubtractBackground(TH1D* h, TF1* bg) {

  //create a clone of h to then turn into a subtraction
  TH1D* hSub = (TH1D*)h->Clone("hSub");

  //subtract away the background
  for(int b=1;b<=h->GetNbinsX();b++)
    {
      double x = h->GetBinCenter(b);
      double y = h->GetBinContent(b);
      double back = bg->Eval(x);

      hSub->SetBinContent(b, max(0.0,y-back));
    }

  return hSub;
}


void FitPhotonPeaks(TH1D* h1, TGraphErrors* g) {

  //find the number of troughs in gTroughs
  int n = g->GetN();
  double diff;
  double totalGain=0;
  double diffErr;
  double totalGainErr = 0;
  
  //store the mean in a vector so we can subtract them
  vector<double> allMeans;
  vector<double> allMeanErr;

  for(int i=0; i<n-1; i++) {
    double xmin, xmax, ymin, ymax;

    g->GetPoint(i, xmin, ymin);
    g->GetPoint(i+1, xmax, ymax);

    cout << endl << "Reading a Gaussian between " << xmin << " and " << xmax << endl << endl;

    TF1 *gausFit = new TF1(Form("PE Fit %d", i), "gaus", xmin, xmax);

    //fit the data and get all data 
    h1->Fit(gausFit, "QR+");
    h1->Draw("SAME");
    
    double amplitude = gausFit->GetParameter(0);
    double mean = gausFit->GetParameter(1);
    double sigma = gausFit->GetParameter(2);
    double meanErr = gausFit->GetParError(1);
    double sigmaErr = gausFit->GetParError(2);

    allMeans.push_back(mean);
    allMeanErr.push_back(meanErr);    

    cout << "Mean is " << mean << " ± " << meanErr << endl << endl;

    //calculate the difference between the peaks from the gaussians (it auto skips the pedestal first peak and skips the last peak but that's fine)
    if(i>0) {
      diff = allMeans[i]-allMeans[i-1];
      diffErr = sqrt(pow(allMeanErr[i],2)+pow(allMeanErr[i-1],2));
      cout << "Gain number " << i << " is " << diff << endl << endl << endl;
      if (diff < 15) {
	cout << "Gain value too low, likely an error, skipping... " << endl;
	continue;
      }
      totalGain += diff;
      totalGainErr += diffErr;
    }
    
  }

  //average the total gain to get the gain and gainErr
  //divide n by 2 because one trough is the beginning and the other is the end so subtracting two of them gives the
  //same number of peak-to-peak relationships 
  double Gain, GainErr;
  Gain = totalGain/(n-2);
  GainErr = totalGainErr/(n-2);

  cout << "The gain of the SiPM is:   " << Gain << " ± " << GainErr << endl << endl;
}



//The following functions are created by H.Pernegger and  Markus Friedl and are avaliable and documented on the ROOT reference guide website. 

double langaufun(double *x, double *par) {
 
   //Fit parameters:
   //par[0]=Width (scale) parameter of Landau density
   //par[1]=Most Probable (MP, location) parameter of Landau density
   //par[2]=Total area (integral -inf to inf, normalization constant)
   //par[3]=Width (sigma) of convoluted Gaussian function
   //
   //In the Landau distribution (represented by the CERNLIB approximation),
   //the maximum is located at x=-0.22278298 with the location parameter=0.
   //This shift is corrected within this function, so that the actual
   //maximum is identical to the MP parameter.
 
      // Numeric constants
      double invsq2pi = 0.3989422804014;   // (2 pi)^(-1/2)
      double mpshift  = -0.22278298;       // Landau maximum location
 
      // Control constants
      double np = 100.0;      // number of convolution steps
      double sc =   5.0;      // convolution extends to +-sc Gaussian sigmas
 
      // Variables
      double xx;
      double mpc;
      double fland;
      double sum = 0.0;
      double xlow,xupp;
      double step;
      double i;
 
 
      // MP shift correction
      mpc = par[1] - mpshift * par[0];
 
      // Range of convolution integral
      xlow = x[0] - sc * par[3];
      xupp = x[0] + sc * par[3];
 
      step = (xupp-xlow) / np;
 
      // Convolution integral of Landau and Gaussian by sum
      for(i=1.0; i<=np/2; i++) {
         xx = xlow + (i-.5) * step;
         fland = TMath::Landau(xx,mpc,par[0]) / par[0];
         sum += fland * TMath::Gaus(x[0],xx,par[3]);
 
         xx = xupp - (i-.5) * step;
         fland = TMath::Landau(xx,mpc,par[0]) / par[0];
         sum += fland * TMath::Gaus(x[0],xx,par[3]);
      }
 
      return (par[2] * step * sum * invsq2pi / par[3]);
}
 
 
 
TF1 *langaufit(TH1F *his, double *fitrange, double *startvalues, double *parlimitslo, double *parlimitshi, double *fitparams, double *fiterrors, double *ChiSqr, int *NDF)
{
   // Once again, here are the Landau * Gaussian parameters:
   //   par[0]=Width (scale) parameter of Landau density
   //   par[1]=Most Probable (MP, location) parameter of Landau density
   //   par[2]=Total area (integral -inf to inf, normalization constant)
   //   par[3]=Width (sigma) of convoluted Gaussian function
   //
   // Variables for langaufit call:
   //   his             histogram to fit
   //   fitrange[2]     lo and hi boundaries of fit range
   //   startvalues[4]  reasonable start values for the fit
   //   parlimitslo[4]  lower parameter limits
   //   parlimitshi[4]  upper parameter limits
   //   fitparams[4]    returns the final fit parameters
   //   fiterrors[4]    returns the final fit errors
   //   ChiSqr          returns the chi square
   //   NDF             returns ndf
 
   int i;
   char FunName[100];
 
   sprintf(FunName,"Fitfcn_%s",his->GetName());
 
   TF1 *ffitold = (TF1*)gROOT->GetListOfFunctions()->FindObject(FunName);
   if (ffitold) delete ffitold;
 
   TF1 *ffit = new TF1(FunName,langaufun,fitrange[0],fitrange[1],4);
   ffit->SetParameters(startvalues);
   ffit->SetParNames("Width","MP","Area","GSigma");
 
   for (i=0; i<4; i++) {
      ffit->SetParLimits(i, parlimitslo[i], parlimitshi[i]);
   }
 
   his->Fit(FunName,"RB0");   // fit within specified range, use ParLimits, do not plot
 
   ffit->GetParameters(fitparams);    // obtain fit parameters
   for (i=0; i<4; i++) {
      fiterrors[i] = ffit->GetParError(i);     // obtain fit parameter errors
   }
   ChiSqr[0] = ffit->GetChisquare();  // obtain chi^2
   NDF[0] = ffit->GetNDF();           // obtain ndf
 
   return (ffit);              // return fit function
 
}
 
 
int langaupro(double *params, double &maxx, double &FWHM) {
 
   // Searches for the location (x value) at the maximum of the
   // Landau-Gaussian convolute and its full width at half-maximum.
   //
   // The search is probably not very efficient, but it's a first try.
 
   double p,x,fy,fxr,fxl;
   double step;
   double l,lold;
   int i = 0;
   int MAXCALLS = 10000;
 
 
   // Search for maximum
 
   p = params[1] - 0.1 * params[0];
   step = 0.05 * params[0];
   lold = -2.0;
   l    = -1.0;
 
 
   while ( (l != lold) && (i < MAXCALLS) ) {
      i++;
 
      lold = l;
      x = p + step;
      l = langaufun(&x,params);
 
      if (l < lold)
         step = -step/10;
 
      p += step;
   }
 
   if (i == MAXCALLS)
      return (-1);
 
   maxx = x;
 
   fy = l/2;
 
 
   // Search for right x location of fy
 
   p = maxx + params[0];
   step = params[0];
   lold = -2.0;
   l    = -1e300;
   i    = 0;
 
 
   while ( (l != lold) && (i < MAXCALLS) ) {
      i++;
 
      lold = l;
      x = p + step;
      l = TMath::Abs(langaufun(&x,params) - fy);
 
      if (l > lold)
         step = -step/10;
 
      p += step;
   }
 
   if (i == MAXCALLS)
      return (-2);
 
   fxr = x;
 
 
   // Search for left x location of fy
 
   p = maxx - 0.5 * params[0];
   step = -params[0];
   lold = -2.0;
   l    = -1e300;
   i    = 0;
 
   while ( (l != lold) && (i < MAXCALLS) ) {
      i++;
 
      lold = l;
      x = p + step;
      l = TMath::Abs(langaufun(&x,params) - fy);
 
      if (l > lold)
         step = -step/10;
 
      p += step;
   }
 
   if (i == MAXCALLS)
      return (-3);
 
 
   fxl = x;
 
   FWHM = fxr - fxl;
   return (0);
}
 
void langaus() {
   // Fill Histogram
   int data[100] = {0,0,0,0,0,0,2,6,11,18,18,55,90,141,255,323,454,563,681,
                    737,821,796,832,720,637,558,519,460,357,291,279,241,212,
                    153,164,139,106,95,91,76,80,80,59,58,51,30,49,23,35,28,23,
                    22,27,27,24,20,16,17,14,20,12,12,13,10,17,7,6,12,6,12,4,
                    9,9,10,3,4,5,2,4,1,5,5,1,7,1,6,3,3,3,4,5,4,4,2,2,7,2,4};
   TH1F *hSNR = new TH1F("snr","Signal-to-noise",400,0,400);
 
   for (int i=0; i<100; i++) hSNR->Fill(i,data[i]);
 
   // Fitting SNR histo
   printf("Fitting...\n");
 
   // Setting fit range and start values
   double fr[2];
   double sv[4], pllo[4], plhi[4], fp[4], fpe[4];
   fr[0]=0.3*hSNR->GetMean();
   fr[1]=3.0*hSNR->GetMean();
 
   pllo[0]=0.5; pllo[1]=5.0; pllo[2]=1.0; pllo[3]=0.4;
   plhi[0]=5.0; plhi[1]=50.0; plhi[2]=1000000.0; plhi[3]=5.0;
   sv[0]=1.8; sv[1]=20.0; sv[2]=50000.0; sv[3]=3.0;
 
   double chisqr;
   int    ndf;
   TF1 *fitsnr = langaufit(hSNR,fr,sv,pllo,plhi,fp,fpe,&chisqr,&ndf);
 
   double SNRPeak, SNRFWHM;
   langaupro(fp,SNRPeak,SNRFWHM);
 
   printf("Fitting done\nPlotting results...\n");
 
   // Global style settings
   gStyle->SetOptStat(1111);
   gStyle->SetOptFit(111);
   gStyle->SetLabelSize(0.03,"x");
   gStyle->SetLabelSize(0.03,"y");
 
   hSNR->GetXaxis()->SetRange(0,70);
   hSNR->Draw();
   fitsnr->Draw("lsame");
}
 
