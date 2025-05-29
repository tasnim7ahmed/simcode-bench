#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/config-store.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WSN_Energy");

double totalEnergyConsumed = 0;
uint32_t totalPacketsSent = 0;

void
ReceivePacket (Ptr<Socket> socket)
{
  Address remoteAddr;
  Ptr<Packet> packet = socket->RecvFrom (remoteAddr);

  totalPacketsSent++;
  NS_LOG_INFO ("Received one packet!");
}

void
InstallReceiver (NodeContainer& sinkNodes, uint16_t port)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sinkSocket = Socket::CreateSocket (sinkNodes.Get (0), tid);
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), port);
  sinkSocket->Bind (local);
  sinkSocket->SetRecvCallback (MakeCallback (&ReceivePacket));
}

void
GenerateTraffic (Ptr<Node> sensorNode, Address sinkAddress, uint32_t packetSize, uint32_t numPackets, DataRate dataRate, Time interPacketInterval)
{
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> sourceSocket = Socket::CreateSocket (sensorNode, tid);

  sourceSocket->Connect (sinkAddress);

  for (uint32_t i = 0; i < numPackets; ++i)
    {
      Ptr<Packet> packet = Create<Packet> (packetSize);
      sourceSocket->Send (packet);
      Simulator::Schedule (interPacketInterval * (i+1), &Socket::Send, sourceSocket, packet);
    }
}

void
CheckEnergy (NodeContainer& sensorNodes, double simulationTime)
{
  bool allNodesDead = true;
  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource> (sensorNodes.Get (i)->GetObject<EnergySource> (0));
      if (energySource->GetRemainingEnergy () > 0)
        {
          allNodesDead = false;
          break;
        }
    }

  if (allNodesDead)
    {
      std::cout << "All nodes have depleted their energy. Stopping simulation." << std::endl;
      std::cout << "Total energy consumed: " << totalEnergyConsumed << std::endl;
      std::cout << "Total packets sent: " << totalPacketsSent << std::endl;
      Simulator::Stop (Seconds (simulationTime + 1));
    }
  else
    {
      Simulator::Schedule (Seconds (1), &CheckEnergy, sensorNodes, simulationTime);
    }
}

void
TotalEnergyConsumption (double oldValue, double newValue)
{
  totalEnergyConsumed += (oldValue - newValue);
}

int
main (int argc, char *argv[])
{
  bool enablePcap = false;
  uint32_t numNodes = 9;
  double simulationTime = 100;
  double nodeRange = 20;
  uint32_t packetSize = 1024;
  uint32_t numPackets = 10;
  DataRate dataRate ("1Mbps");
  Time interPacketInterval = Seconds (0.1);
  double initialEnergy = 100;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("numNodes", "Number of sensor nodes", numNodes);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("packetSize", "Size of each packet in bytes", packetSize);
  cmd.AddValue ("numPackets", "Number of packets to send", numPackets);
  cmd.AddValue ("initialEnergy", "Initial energy of each node", initialEnergy);

  cmd.Parse (argc, argv);

  Config::SetDefault ("ns3::WifiMac::Ssid", StringValue ("ns-3-ssid"));

  NodeContainer sinkNodes;
  sinkNodes.Create (1);

  NodeContainer sensorNodes;
  sensorNodes.Create (numNodes);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer sinkDevices = wifi.Install (wifiPhy, wifiMac, sinkNodes);
  NetDeviceContainer sensorDevices = wifi.Install (wifiPhy, wifiMac, sensorNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (nodeRange),
                                 "DeltaY", DoubleValue (nodeRange),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  NodeContainer allNodes;
  allNodes.Add (sinkNodes);
  allNodes.Add (sensorNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (allNodes);

  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer sinkInterfaces = ipv4.Assign (sinkDevices);
  Ipv4InterfaceContainer sensorInterfaces = ipv4.Assign (sensorDevices);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Energy Model
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("SupplyVoltage", DoubleValue (3.3));
  basicSourceHelper.Set ("InitialEnergy", DoubleValue (initialEnergy));

  EnergySourceContainer sources = basicSourceHelper.Install (sensorNodes);

  WifiRadioEnergyModelHelper radioEnergyHelper;
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  EnergySourceContainer deviceSources = radioEnergyHelper.Install (sensorDevices, sources);

  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      Ptr<BasicEnergySource> energySource = DynamicCast<BasicEnergySource> (sources.Get (i));
      energySource->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&TotalEnergyConsumption));
    }

  // Traffic generation
  uint16_t sinkPort = 9;
  InstallReceiver (sinkNodes, sinkPort);

  for (uint32_t i = 0; i < sensorNodes.GetN (); ++i)
    {
      InetSocketAddress sinkAddress (sinkInterfaces.GetAddress (0), sinkPort);
      Simulator::Schedule (Seconds (i*0.1), &GenerateTraffic, sensorNodes.Get (i), sinkAddress, packetSize, numPackets, dataRate, interPacketInterval);
    }

  Simulator::Schedule (Seconds (1), &CheckEnergy, sensorNodes, simulationTime);

  if (enablePcap)
    {
      wifiPhy.EnablePcapAll ("wsn_energy");
    }

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}