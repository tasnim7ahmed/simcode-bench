#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-apps-module.h"

NS_LOG_COMPONENT_DEFINE ("WifiHttpSimulation");

int main (int argc, char *argv[])
{
  double simulationTime = 10.0;
  double serverStartTime = 1.0;
  double clientStartTime = 2.0;
  uint16_t httpPort = 80;

  ns3::NodeContainer nodes;
  nodes.Create (2);

  ns3::MobilityHelper mobility;
  ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator> ();
  positionAlloc->Add (ns3::Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (ns3::Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  ns3::WifiHelper wifi;
  wifi.SetStandard (ns3::WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", ns3::StringValue ("Dsss11Mbps"));

  ns3::YansWifiChannelHelper channel;
  channel.SetPropagationLossModel ("ns3::LogDistancePropagationLossModel");

  ns3::YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  ns3::WifiMacHelper mac;
  mac.SetType ("ns3::AdhocWifiMac");

  ns3::NetDeviceContainer devices;
  devices = wifi.Install (phy, mac, nodes);

  ns3::InternetStackHelper stack;
  stack.Install (nodes);

  ns3::Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  ns3::Ipv4InterfaceContainer interfaces = address.Assign (devices);

  ns3::Ptr<ns3::HttpRequestor::DocumentCreator> serverDocCreator = ns3::CreateObject<ns3::HttpRequestor::DocumentCreator> ();
  serverDocCreator->AddDocument ("/", "<html><body><h1>Hello NS-3!</h1></body></html>");
  
  ns3::ApplicationContainer serverApp;
  ns3::HttpResponseorHelper serverHelper (httpPort);
  serverHelper.SetAttribute ("DocumentCreator", ns3::PointerValue (serverDocCreator));

  serverApp = serverHelper.Install (nodes.Get (0));
  serverApp.Start (ns3::Seconds (serverStartTime));
  serverApp.Stop (ns3::Seconds (simulationTime));

  ns3::ApplicationContainer clientApp;
  ns3::HttpRequestorHelper clientHelper (interfaces.GetAddress (0), httpPort, "/"); 
  clientHelper.SetAttribute ("NumRequests", ns3::UintegerValue (1));
  clientHelper.SetAttribute ("RequestTimeout", ns3::TimeValue (ns3::Seconds (5.0)));

  clientApp = clientHelper.Install (nodes.Get (1));
  clientApp.Start (ns3::Seconds (clientStartTime));
  clientApp.Stop (ns3::Seconds (simulationTime));

  phy.EnablePcap ("wifi-http-sim", devices);

  ns3::Simulator::Stop (ns3::Seconds (simulationTime));
  ns3::Simulator::Run ();
  ns3::Simulator::Destroy ();

  return 0;
}