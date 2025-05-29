#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WsnEnergyGridSimulation");

static bool
HasAliveNodes (Ptr<NodeContainer> nodes, NodeContainer& sinkNode)
{
  for (uint32_t i = 0; i < nodes->GetN (); ++i)
    {
      if (nodes->Get (i) == sinkNode.Get (0))
        continue;
      Ptr<EnergySourceContainer> energySources = DynamicCast<EnergySourceContainer> (nodes->Get (i)->GetObject<EnergySourceContainer> ());
      if (energySources != nullptr && energySources->GetN () > 0)
        {
          Ptr<BasicEnergySource> es = DynamicCast<BasicEnergySource> (energySources->Get (0));
          if (es != nullptr && es->GetRemainingEnergy () > 0)
            {
              return true;
            }
        }
      else
        {
          Ptr<BasicEnergySource> es = nodes->Get (i)->GetObject<BasicEnergySource> ();
          if (es != nullptr && es->GetRemainingEnergy () > 0)
            {
              return true;
            }
        }
    }
  return false;
}

void
CheckNetworkEnergy (Ptr<NodeContainer> sensorNodes, NodeContainer& sinkNode)
{
  if (!HasAliveNodes (sensorNodes, sinkNode))
    {
      NS_LOG_UNCOND ("All sensor nodes depleted energy. Stopping simulation at " << Simulator::Now ().GetSeconds () << "s");
      Simulator::Stop ();
    }
  else
    {
      Simulator::Schedule (Seconds (1.0), &CheckNetworkEnergy, sensorNodes, std::ref(sinkNode));
    }
}

int
main (int argc, char *argv[])
{
  uint32_t gridSizeX = 4;
  uint32_t gridSizeY = 4;
  double nodeSpacing = 20.0;
  uint32_t numNodes = gridSizeX * gridSizeY;
  double initialEnergyJ = 1.0;
  double sinkX = (gridSizeX-1) * nodeSpacing / 2.0;
  double sinkY = (gridSizeY) * nodeSpacing + 30.0;

  CommandLine cmd;
  cmd.AddValue ("gridX", "Grid size X", gridSizeX);
  cmd.AddValue ("gridY", "Grid size Y", gridSizeY);
  cmd.AddValue ("nodeSpacing", "Spacing between nodes", nodeSpacing);
  cmd.AddValue ("initialEnergyJ", "Initial energy (Joule)", initialEnergyJ);
  cmd.Parse (argc, argv);

  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (nodeSpacing),
                                 "DeltaY", DoubleValue (nodeSpacing),
                                 "GridWidth", UintegerValue (gridSizeX),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  Ptr<ListPositionAllocator> sinkPos = CreateObject<ListPositionAllocator> ();
  sinkPos->Add (Vector (sinkX, sinkY, 0.0));
  mobility.SetPositionAllocator (sinkPos);
  mobility.Install (sinkNode);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("wsn-grid");

  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer sensorDevices = wifi.Install (phy, mac, sensorNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer sinkDevice = wifi.Install (phy, mac, sinkNode);

  InternetStackHelper stack;
  stack.Install (sensorNodes);
  stack.Install (sinkNode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorIfs = address.Assign (sensorDevices);
  Ipv4InterfaceContainer sinkIfs = address.Assign (sinkDevice);

  BasicEnergySourceHelper energySourceHelper;
  energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (initialEnergyJ));
  EnergySourceContainer energySources = energySourceHelper.Install (sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174)); // ~17.4 mA for Tx
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197)); // ~19.7 mA for Rx
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (sensorDevices, energySources);

  uint16_t port = 8080;
  Address sinkAddress (InetSocketAddress (sinkIfs.GetAddress (0), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = packetSinkHelper.Install (sinkNode);
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10000.0));

  uint32_t packetSize = 32;
  double interPacketInterval = 2.0;
  ApplicationContainer sensorApps;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      if (sensorNodes.Get (i) == sinkNode.Get (0))
        {
          continue;
        }

      UdpSocketFactoryHelper udpFactoryHelper;
      Ptr<Socket> sourceSocket = Socket::CreateSocket (sensorNodes.Get (i), UdpSocketFactory::GetTypeId ());
      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      double appStart = 1.0 + var->GetValue (0.0, 0.2); // Staggered start

      Ptr<Application> app =
        CreateObject<OnOffApplication> ();
      app->SetAttribute ("Remote", AddressValue (sinkAddress));
      app->SetAttribute ("PacketSize", UintegerValue (packetSize));
      app->SetAttribute ("DataRate", StringValue ("1kbps"));
      app->SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
      app->SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=" + std::to_string (interPacketInterval-1) + "]"));
      sensorNodes.Get (i)->AddApplication (app);
      app->SetStartTime (Seconds (appStart));
      app->SetStopTime (Seconds (10000.0));
      sensorApps.Add (app);
    }

  Simulator::Schedule (Seconds (1.0), &CheckNetworkEnergy, Ptr<NodeContainer> (&sensorNodes), std::ref (sinkNode));
  Simulator::Stop (Seconds (10000.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}