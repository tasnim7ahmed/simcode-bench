#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiEnergyDemo");

void RemainingEnergyTrace (double oldValue, double remainingEnergy)
{
  NS_LOG_UNCOND ("Remaining energy: " << remainingEnergy << " J at " << Simulator::Now ().GetSeconds () << "s");
}

void TotalEnergyConsumptionTrace (double oldValue, double totalConsumed)
{
  NS_LOG_UNCOND ("Total energy consumed: " << totalConsumed << " J at " << Simulator::Now ().GetSeconds () << "s");
}

int main (int argc, char *argv[])
{
  // Parameter defaults
  double simStartTime = 1.0;
  double simStopTime = 10.0;
  uint32_t packetSize = 1024;
  double nodeDistance = 20.0; // meters
  
  // Command line parsing
  CommandLine cmd;
  cmd.AddValue ("packetSize", "Packet size in bytes", packetSize);
  cmd.AddValue ("simStartTime", "Simulation application start time in seconds", simStartTime);
  cmd.AddValue ("nodeDistance", "Distance between nodes (meters)", nodeDistance);
  cmd.Parse (argc, argv);
  
  // Nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Wifi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  WifiMacHelper mac;
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  Ssid ssid = Ssid ("energy-demo");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevice = wifi.Install (phy, mac, nodes.Get (0));

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, nodes.Get (1));

  NetDeviceContainer allDevices;
  allDevices.Add (staDevice);
  allDevices.Add (apDevice);

  // Mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (nodeDistance, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Energy model
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (0.1));
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (allDevices, sources);

  // Traces for energy
  for (uint32_t i = 0; i < sources.GetN (); ++i)
    {
      sources.Get (i)->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergyTrace));
      Ptr<WifiRadioEnergyModel> wifiModel = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (i));
      if (wifiModel)
        wifiModel->TraceConnectWithoutContext ("TotalEnergyConsumption", MakeCallback (&TotalEnergyConsumptionTrace));
    }
  
  // Internet stack
  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (allDevices);

  // UDP Applications
  uint16_t port = 9;
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (simStartTime));
  serverApp.Stop (Seconds (simStopTime));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10000));
  client.SetAttribute ("Interval", TimeValue (MilliSeconds (50)));
  client.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (simStartTime));
  clientApp.Stop (Seconds (simStopTime));

  // Logging packet transmit details
  Config::Connect ("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/State/TxStart", 
      MakeCallback ([] (std::string context, Ptr<const Packet> p) {
        NS_LOG_UNCOND ("Packet transmitted at " << Simulator::Now ().GetSeconds () 
          << "s, size: " << p->GetSize () << " bytes");
      }));

  Simulator::Stop (Seconds (simStopTime));

  // Run the simulation
  Simulator::Run ();

  // Energy usage check (limits enforcement)
  double consumed0 = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (0))->GetTotalEnergyConsumption ();
  double consumed1 = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (1))->GetTotalEnergyConsumption ();
  if (consumed0 > 0.1 || consumed1 > 0.1)
    NS_LOG_UNCOND ("ERROR: Energy consumed exceeds available energy!");

  Simulator::Destroy ();
  return 0;
}