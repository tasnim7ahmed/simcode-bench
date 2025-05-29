#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OfdmErrorRateModels");

// Function to convert SNR from dB to linear scale
double
SnrDbToLinear (double snrDb)
{
  return pow (10, snrDb / 10);
}

int
main (int argc, char *argv[])
{
  // Set default values for simulation parameters
  uint32_t frameSize = 1000; // bytes
  double simulationTime = 10.0; // seconds
  double snrStart = -5.0; // dB
  double snrStop = 30.0; // dB
  uint32_t numSnrPoints = 36;
  std::string rateName = "OfdmRate6MbpsBody";

  // Enable command-line arguments parsing
  CommandLine cmd;
  cmd.AddValue ("frameSize", "Frame size in bytes", frameSize);
  cmd.Parse (argc, argv);

  // Enable logging
  LogComponentEnable ("OfdmErrorRateModels", LOG_LEVEL_INFO);

  // Create output file for Gnuplot
  std::ofstream outputFile ("ofdm_error_rate_models.dat");
  if (!outputFile.is_open ())
    {
      std::cerr << "Error: Could not open output file." << std::endl;
      return 1;
    }

  // Configure simulation parameters
  double snrStep = (snrStop - snrStart) / (numSnrPoints - 1);

  // Loop through different SNR values
  for (uint32_t i = 0; i < numSnrPoints; ++i)
    {
      double snrDb = snrStart + i * snrStep;
      double snrLinear = SnrDbToLinear (snrDb);

      // Create the channel
      Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel> ();
      // Create the propagation loss model
      Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel> ();
      wifiChannel->SetPropagationDelayModel (delayModel);
      Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel> ();
      lossModel->SetPathLossExponent (3);
      lossModel->SetReference (1, 7.7623);
      wifiChannel->SetPropagationLossModel (lossModel);

      // Create the PHY and the MAC
      YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
      wifiPhy.SetChannel (wifiChannel);

      WifiMacHelper wifiMac;
      wifiMac.SetType ("ns3::AdhocWifiMac");
      Ssid ssid = Ssid ("ns-3-ssid");

      WifiHelper wifiHelper;
      wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211a);
      wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                         "DataMode", StringValue (rateName),
                                         "ControlMode", StringValue (rateName));

      // Create nodes
      NodeContainer nodes;
      nodes.Create (2);

      // Install PHY and MAC
      NetDeviceContainer devices = wifiHelper.Install (wifiPhy, wifiMac, nodes);

      // Configure mobility
      MobilityHelper mobility;
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (0.0),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (10.0),
                                     "GridWidth", UintegerValue (3),
                                     "LayoutType", StringValue ("RowFirst"));
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (nodes);

      // Install Internet stack
      InternetStackHelper stack;
      stack.Install (nodes);

      Ipv4AddressHelper address;
      address.SetBase ("10.1.1.0", "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign (devices);

      // Create UDP traffic
      uint16_t port = 9;
      UdpEchoServerHelper echoServer (port);
      ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
      serverApps.Start (Seconds (0.0));
      serverApps.Stop (Seconds (simulationTime));

      UdpEchoClientHelper echoClient (interfaces.GetAddress (1), port);
      echoClient.SetAttribute ("MaxPackets", UintegerValue (1000000));
      echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.0001))); // Adjust interval for sufficient packets
      echoClient.SetAttribute ("PacketSize", UintegerValue (frameSize));
      ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
      clientApps.Start (Seconds (1.0));
      clientApps.Stop (Seconds (simulationTime));

      // Configure error rate models
      // (NIST, YANS, Table-based error rate models)
      // Here, use the default error rate model without explicit configuration.
      // The simulation will use the error rate model configured in the YansWifiPhyHelper.

      // Run the simulation
      Simulator::Stop (Seconds (simulationTime + 1));
      Simulator::Run ();

      // Get statistics
      Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient> (clientApps.Get (0));
      uint32_t sent = client->GetPacketsSent ();
      uint32_t received = client->GetPacketsReceived ();
      double frameSuccessRate = (double) received / sent;

      // Write results to file
      outputFile << snrDb << " " << frameSuccessRate << std::endl;

      Simulator::Destroy ();
    }

  // Close the output file
  outputFile.close ();

  // Generate Gnuplot script
  std::ofstream gnuplotScript ("ofdm_error_rate_models.plt");
  if (gnuplotScript.is_open ())
    {
      gnuplotScript << "set terminal png size 1024,768" << std::endl;
      gnuplotScript << "set output 'ofdm_error_rate_models.png'" << std::endl;
      gnuplotScript << "set title 'Frame Success Rate vs. SNR for OFDM'" << std::endl;
      gnuplotScript << "set xlabel 'SNR (dB)'" << std::endl;
      gnuplotScript << "set ylabel 'Frame Success Rate'" << std::endl;
      gnuplotScript << "set grid" << std::endl;
      gnuplotScript << "plot 'ofdm_error_rate_models.dat' using 1:2 title '" << rateName << "' with lines" << std::endl;
      gnuplotScript.close ();
    }
  else
    {
      std::cerr << "Error: Could not open Gnuplot script file." << std::endl;
      return 1;
    }

  std::cout << "Simulation finished. Results are in ofdm_error_rate_models.dat and plot is in ofdm_error_rate_models.png" << std::endl;

  return 0;
}