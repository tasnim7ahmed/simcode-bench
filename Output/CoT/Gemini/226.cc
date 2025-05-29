#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/energy-module.h"
#include "ns3/uinteger.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-client.h"
#include "ns3/udp-server.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN");

int main (int argc, char *argv[])
{
  bool verbose = false;
  uint32_t numNodes = 10;
  double simulationTime = 50; //seconds
  double sinkX = 50.0;
  double sinkY = 50.0;

  CommandLine cmd;
  cmd.AddValue ("verbose", "Tell application to log if true", verbose);
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
    }

  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);

  NodeContainer sinkNode;
  sinkNode.Create (1);

  NodeContainer allNodes;
  allNodes.Add (sensorNodes);
  allNodes.Add (sinkNode);

  WifiHelper wifi;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-wpan");
  mac.SetType ("ns3::AdhocWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer sensorDevices = wifi.Install (phy, mac, sensorNodes);
  NetDeviceContainer sinkDevice = wifi.Install (phy, mac, sinkNode);

  InternetStackHelper internet;
  internet.Install (allNodes);

  OlsrHelper olsr;
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);
  Ipv4InterfaceContainer sinkInterface = ipv4.Assign (sinkDevice);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=50.0]"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (sensorNodes);

  Ptr<Node> sink = sinkNode.Get (0);
  Ptr<ConstantPositionMobilityModel> sinkMobility = sink->GetObject<ConstantPositionMobilityModel> ();
  sinkMobility->SetPosition (Vector (sinkX, sinkY, 0));

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (sinkNode.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  recvSink->Bind (local);

  UdpServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (sinkNode.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime - 1));

  UdpClientHelper echoClient (sinkInterface.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < numNodes; ++i)
    {
      clientApps.Add (echoClient.Install (sensorNodes.Get (i)));
    }
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime - 2));

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  EnergySourceContainer sources = basicSourceHelper.Install (allNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  EnergySourceContainer deviceSources = radioEnergyHelper.Install (sensorDevices);
  deviceSources.Add (radioEnergyHelper.Install (sinkDevice));

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}