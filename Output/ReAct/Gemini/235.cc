#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lorawan-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("LoRaWanSensorSink");

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

  NodeContainer nodes;
  nodes.Create (2);

  Ptr<LoraWanPhy> phySensor = CreateObject<LoraWanPhy> ();
  Ptr<LoraWanPhy> phySink = CreateObject<LoraWanPhy> ();

  phySensor->SetDeviceType (LoraWanPhy::DeviceType::END_DEVICE);
  phySink->SetDeviceType (LoraWanPhy::DeviceType::GATEWAY);

  Ptr<LoraWanMac> macSensor = CreateObject<LoraWanMac> ();
  Ptr<LoraWanMac> macSink = CreateObject<LoraWanMac> ();

  Ptr<LoraWanNetDevice> netDeviceSensor = CreateObject<LoraWanNetDevice> ();
  Ptr<LoraWanNetDevice> netDeviceSink = CreateObject<LoraWanNetDevice> ();

  netDeviceSensor->SetPhy (phySensor);
  netDeviceSensor->SetMac (macSensor);
  nodes.Get (0)->AddDevice (netDeviceSensor);

  netDeviceSink->SetPhy (phySink);
  netDeviceSink->SetMac (macSink);
  nodes.Get (1)->AddDevice (netDeviceSink);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
                                 "X", StringValue ("50.0"),
                                 "Y", StringValue ("50.0"),
                                 "Rho", StringValue ("0"),
                                 "DeltaRho", StringValue ("10"),
                                 "Theta", StringValue ("0"),
                                 "DeltaTheta", StringValue ("360"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper internet;
  internet.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (nodes);

  uint16_t port = 9;

  UdpServerHelper server (port);
  ApplicationContainer apps = server.Install (nodes.Get (1));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  apps = client.Install (nodes.Get (0));
  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (11.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}