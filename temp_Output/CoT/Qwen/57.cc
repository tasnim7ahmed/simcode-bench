#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("StarTopologySimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("OnOffApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PacketSink", LOG_LEVEL_INFO);

  NodeContainer hub;
  hub.Create (1);

  NodeContainer spokes;
  spokes.Create (8);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer hubDevices;
  NetDeviceContainer spokeDevices;

  for (uint32_t i = 0; i < spokes.GetN (); ++i)
    {
      NetDeviceContainer link = p2p.Install (hub.Get (0), spokes.Get (i));
      hubDevices.Add (link.Get (0));
      spokeDevices.Add (link.Get (1));
    }

  InternetStackHelper stack;
  stack.InstallAll ();

  Ipv4AddressHelper address;
  Ipv4InterfaceContainer hubInterfaces;
  Ipv4InterfaceContainer spokeInterfaces;

  for (uint32_t i = 0; i < spokes.GetN (); ++i)
    {
      std::ostringstream subnet;
      subnet << "10.0." << i << ".0";
      address.SetBase (subnet.str ().c_str (), "255.255.255.0");
      Ipv4InterfaceContainer interfaces = address.Assign (NetDeviceContainer (hubDevices.Get (i), spokeDevices.Get (i)));
      hubInterfaces.Add (interfaces.Get (0));
      spokeInterfaces.Add (interfaces.Get (1));
    }

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 50000;

  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                               InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApp = sinkHelper.Install (hub.Get (0));
  sinkApp.Start (Seconds (0.0));
  sinkApp.Stop (Seconds (10.0));

  OnOffHelper onoff ("ns3::TcpSocketFactory",
                     InetSocketAddress (hubInterfaces.GetAddress (0), sinkPort));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate ("14kbps")));
  onoff.SetAttribute ("PacketSize", UintegerValue (137));

  ApplicationContainer spokeApps;

  for (uint32_t i = 0; i < spokes.GetN (); ++i)
    {
      ApplicationContainer app = onoff.Install (spokes.Get (i));
      app.Start (Seconds (1.0));
      app.Stop (Seconds (10.0));
      spokeApps.Add (app);
    }

  for (uint32_t i = 0; i < hubDevices.GetN (); ++i)
    {
      std::ostringstream filename;
      filename << "star-topology-hub-" << i << ".pcap";
      p2p.EnablePcap (filename.str ().c_str (), hubDevices.Get (i), false);
    }

  for (uint32_t i = 0; i < spokeDevices.GetN (); ++i)
    {
      std::ostringstream filename;
      filename << "star-topology-spoke-" << i << ".pcap";
      p2p.EnablePcap (filename.str ().c_str (), spokeDevices.Get (i), false);
    }

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}