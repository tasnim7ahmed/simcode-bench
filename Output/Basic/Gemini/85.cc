#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/lte-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/log.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/buildings-module.h"
#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("VehicularNetworkSimulation");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("VehicularNetworkSimulation", LOG_LEVEL_INFO);

  // Define simulation parameters
  uint32_t numVehicles = 20;
  double simulationTime = 10.0;
  double vehicleSpeed = 20.0; // m/s
  double distance = 100.0;    // initial distance between vehicles

  // Create nodes
  NodeContainer vehicles;
  vehicles.Create (numVehicles);

  // Mobility Model (Highway Scenario)
  MobilityHelper mobilityHighway;
  mobilityHighway.SetPositionAllocator ("ns3::GridPositionAllocator",
                                         "MinX", DoubleValue (0.0),
                                         "MinY", DoubleValue (0.0),
                                         "DeltaX", DoubleValue (distance),
                                         "DeltaY", DoubleValue (0.0),
                                         "GridWidth", UintegerValue (numVehicles),
                                         "LayoutType", StringValue ("RowFirst"));
  mobilityHighway.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobilityHighway.Install (vehicles);

  for (uint32_t i = 0; i < vehicles.GetN (); ++i)
    {
      Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get (i)->GetObject<ConstantVelocityMobilityModel> ();
      mob->SetVelocity (Vector (vehicleSpeed, 0, 0));
    }


  // Channel and Propagation Model (3GPP TR 38.901)
  Ptr<LteUeSpectrumPhy> lteUePhy;
  Ptr<LteEnbSpectrumPhy> lteEnbPhy;

  // Create channel
  TypeIdValue spectrumPropagationLossModelType;
  spectrumPropagationLossModelType.Set ("ns3::ThreeGppSpectrumPropagationLossModel");

  ObjectFactory factory;
  factory.SetTypeId (spectrumPropagationLossModelType.Get ());
  Ptr<ThreeGppSpectrumPropagationLossModel> propagationLossModel = factory.Create<ThreeGppSpectrumPropagationLossModel> ();

  propagationLossModel->SetAttribute ("Scenario", EnumValue (ThreeGppSpectrumPropagationLossModel::Scenario::UMa));

  Ptr<SpectrumChannel> spectrumChannel = CreateObject<SpectrumChannel> ();
  spectrumChannel->SetPropagationLossModel (propagationLossModel);
  spectrumChannel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());

  // LTE Devices
  NetDeviceContainer lteUeDevs, lteEnbDevs;
  LteHelper lteHelper;

  NodeContainer enbNodes;
  enbNodes.Create (1);
  MobilityHelper enbMobility;
  enbMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  enbMobility.Install (enbNodes);
  Ptr<ConstantPositionMobilityModel> enbMobilityModel = enbNodes.Get (0)->GetObject<ConstantPositionMobilityModel> ();
  enbMobilityModel->SetPosition (Vector(0.0, 100.0, 0.0)); // Place the eNB

  lteEnbDevs = lteHelper.InstallEnbDevice (enbNodes);

  lteUeDevs = lteHelper.InstallUeDevice (vehicles);

  // Assign antenna arrays (UPA)
  // (Simplified for brevity - real implementation needs specific antenna pattern calculations)

  // Install the SpectrumPhy
  lteHelper.Attach (lteUeDevs, lteEnbDevs.Get (0));

  // Activate default EPS bearer
  lteHelper.ActivateDataRadioBearer (vehicles, EpsBearer::Qci::GBR_CONV_VOICE);

  // Application (e.g., UDP echo)
  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (vehicles.Get (0));
  serverApps.Start (Seconds (0.1));
  serverApps.Stop (Seconds (simulationTime));

  UdpEchoClientHelper echoClient (vehicles.Get (0)->GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install (vehicles.Get (1));
  clientApps.Start (Seconds (0.2));
  clientApps.Stop (Seconds (simulationTime));

  // Beamforming (simplified DFT) - placeholder
  // Actual beamforming implementation is complex and depends on antenna array configuration and signal processing.

  // Simulation Tracing and Logging
  FlowMonitorHelper flowMonitor;
  Ptr<FlowMonitor> monitor = flowMonitor.InstallAll ();

  Simulator::Stop (Seconds (simulationTime));
  std::ofstream ascii;
  ascii.open("simulation_output.txt");
  Simulator::Schedule (Seconds(simulationTime-1.0), [&](){
      monitor->CheckForLostPackets ();
      Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowMonitor.GetClassifier ());
      FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
          Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
          ascii << "Flow ID: " << i->first  << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress << " Src Port " << t.sourcePort << " Dst Port " << t.destinationPort << "\n";
          ascii << "  Tx Packets: " << i->second.txPackets << "\n";
          ascii << "  Rx Packets: " << i->second.rxPackets << "\n";
          ascii << "  Lost Packets: " << i->second.lostPackets << "\n";
          ascii << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()) / 1024  << " kbps\n";
        }
  });

  Simulator::Run ();

  monitor->SerializeToXmlFile("flowmon.xml", true, true);
  Simulator::Destroy ();
  ascii.close();

  return 0;
}