#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mesh-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("MeshSimpleExample");

int main (int argc, char *argv[])
{
  Time::SetResolution (Time::NS);

  uint32_t numNodes = 3;
  double simulationTime = 10.0; // seconds

  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (numNodes);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  MeshHelper mesh = MeshHelper::Default ();
  mesh.SetStackInstaller ("ns3::Dot11sStack");
  mesh.SetSpreadInterfaceChannels (MeshHelper::SPREAD_CHANNELS);
  mesh.SetMacType ("RandomStart", TimeValue (Seconds (0.1)));
  mesh.SetNumberOfInterfaces (1);

  NetDeviceContainer meshDevices = mesh.Install (wifiPhy, nodes);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (50.0, 0.0, 0.0));
  positionAlloc->Add (Vector (25.0, 43.3, 0.0)); // form an equilateral triangle
  mobility.SetPositionAllocator (positionAlloc);
  mobility.Install (nodes);

  InternetStackHelper internetStack;
  internetStack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (meshDevices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2)); // Node 2 is server
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (20));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (64));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient2 (interfaces.GetAddress (0), 9);
  echoClient2.SetAttribute ("MaxPackets", UintegerValue (20));
  echoClient2.SetAttribute ("Interval", TimeValue (Seconds (0.5)));
  echoClient2.SetAttribute ("PacketSize", UintegerValue (64));

  ApplicationContainer clientApps2 = echoClient2.Install (nodes.Get (1));
  clientApps2.Start (Seconds (2.5));
  clientApps2.Stop (Seconds (simulationTime));

  wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.EnablePcapAll ("mesh");

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  Simulator::Run ();

  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << "Flow statistics:" << std::endl;
  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (it->first);
      std::cout << "Flow ID: " << it->first
                << " Src Addr: " << t.sourceAddress
                << " Dst Addr: " << t.destinationAddress << std::endl;
      std::cout << "  Tx Packets: " << it->second.txPackets << std::endl;
      std::cout << "  Rx Packets: " << it->second.rxPackets << std::endl;
      std::cout << "  Tx Bytes:   " << it->second.txBytes << std::endl;
      std::cout << "  Rx Bytes:   " << it->second.rxBytes << std::endl;
      if (it->second.rxPackets > 0)
        {
          std::cout << "  Mean Delay:  "
                    << (it->second.delaySum.GetSeconds () / it->second.rxPackets) << " s" << std::endl;
        }
      else
        {
          std::cout << "  Mean Delay:  N/A" << std::endl;
        }
    }

  Simulator::Destroy ();
  return 0;
}