#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMeshExample");

int main (int argc, char *argv[])
{
  bool enablePcap = true;
  double simulationTime = 10.0;

  CommandLine cmd;
  cmd.AddValue ("enablePcap", "Enable PCAP tracing", enablePcap);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  NodeContainer meshNodes;
  meshNodes.Create (3);

  MeshHelper mesh;
  mesh.SetStandard (WIFI_PHY_STANDARD_80211s);
  mesh.SetOperatingMode (MeshHelper::OCB);
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);

  mesh.SetMacType ("RandomStart");

  NetDeviceContainer meshDevices = mesh.Install (meshNodes);

  InternetStackHelper internet;
  internet.Install (meshNodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (meshNodes);


  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (meshNodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (meshNodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  if (enablePcap)
    {
      for (uint32_t i = 0; i < meshNodes.GetN (); ++i)
        {
          Ptr<Node> node = meshNodes.Get (i);
          for (uint32_t j = 0; j < node->GetNNetDevice (); ++j)
            {
              Ptr<NetDevice> device = node->GetNetDevice (j);
              PcapHelper pcapHelper;
              pcapHelper.EnablePcapAll ("mesh-node-" + std::to_string (i), device, false);
            }
        }
    }

  AnimationInterface anim ("mesh-animation.xml");

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}