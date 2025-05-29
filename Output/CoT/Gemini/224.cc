#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/internet-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/uinteger.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ManetSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponent::SetDefaultLogLevel (LogLevel::LOG_LEVEL_INFO);
  LogComponent::SetAll (LogLevel::LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (10);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Simple ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  AodvHelper aodv;
  InternetStackHelper stack;
  stack.SetRoutingProtocol (aodv);
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("100.0"),
                                 "Y", StringValue ("100.0"),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=10.0]"),
                             "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=2.0]"));
  mobility.Install (nodes);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");

  for (uint32_t i = 0; i < nodes.GetN (); ++i)
    {
      Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
      rand->SetStream (i);
      Simulator::Schedule (Seconds (rand->GetValue (0.0, 5.0)), [&, i, rand](){
         Ptr<Socket> sink = Socket::CreateSocket (nodes.Get (i), tid);
         InetSocketAddress remote = InetSocketAddress (interfaces.GetAddress (rand->GetInteger (0, nodes.GetN ()-1)), 9);
          sink->Connect (remote);
          Simulator::Schedule (Seconds(rand->GetValue (1.0, 3.0)), [&, sink, rand](){
            Ptr<Packet> packet = Create<Packet> ((uint8_t*) "Hello World", 12);
            sink->Send (packet);
            Simulator::Schedule (Seconds(rand->GetValue (1.0, 3.0)), std::bind(&Socket::Close, sink));
          });
      });
    }

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes);
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}