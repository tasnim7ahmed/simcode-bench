#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/internet-module.h"
#include "ns3/on-off-application.h"
#include "ns3/yans-wifi-phy.h"
#include "ns3/command-line.h"
#include <iostream>
#include <fstream>

using namespace ns3;

static void
DeviceSleep (std::string context, Ptr<EnergySource> energySource)
{
  NS_LOG_UNCOND ("Device enters sleep state: " << context);
}

int main (int argc, char *argv[])
{
  bool tracing = false;
  std::string dataRate = "11Mb/s";
  uint32_t packetSize = 1024;
  uint32_t maxPackets = 0;
  double txPowerStart = 16.0206;
  double txPowerEnd = 16.0206;
  double txPowerLevels = 1;
  double energyConsumption_0 = 0.03;
  double energyConsumption_1 = 0.03;
  double sleepPower = 0.0001;
  double txCurrentA = 0.03;
  double rxCurrentA = 0.02;

  CommandLine cmd;
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);
  cmd.AddValue ("dataRate", "Data rate for OnOff application", dataRate);
  cmd.AddValue ("packetSize", "Packet size for OnOff application", packetSize);
  cmd.AddValue ("maxPackets", "Max Packets for OnOff application", maxPackets);
  cmd.AddValue ("txPowerStart", "Tx Power Start", txPowerStart);
  cmd.AddValue ("txPowerEnd", "Tx Power End", txPowerEnd);
  cmd.AddValue ("txPowerLevels", "Number of Tx Power levels", txPowerLevels);
  cmd.AddValue ("energyConsumption_0", "Node 0 consumption", energyConsumption_0);
  cmd.AddValue ("energyConsumption_1", "Node 1 consumption", energyConsumption_1);
  cmd.AddValue ("sleepPower", "Sleep Power Consumption", sleepPower);
  cmd.AddValue ("txCurrentA", "Tx Current A", txCurrentA);
  cmd.AddValue ("rxCurrentA", "Rx Current A", rxCurrentA);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);

  NodeContainer nodes;
  nodes.Create (2);

  WifiHelper wifi;
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (interfaces.GetAddress (1), 9)));
  onoff.SetConstantRate (DataRate (dataRate), packetSize);
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSize));
  if (maxPackets > 0)
  {
    onoff.SetAttribute ("MaxPackets", UintegerValue (maxPackets));
  }

  ApplicationContainer apps = onoff.Install (nodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

  EnergySourceContainer sources;
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("InitialEnergyJoule", DoubleValue (100.0));
  sources = basicSourceHelper.Install (nodes);

  WifiRadioEnergyModelHelper radioEnergyModelHelper;
  radioEnergyModelHelper.Set ("TxCurrentA", DoubleValue (txCurrentA));
  radioEnergyModelHelper.Set ("RxCurrentA", DoubleValue (rxCurrentA));
  radioEnergyModelHelper.Set ("SleepCurrentA", DoubleValue (sleepPower));
  radioEnergyModelHelper.Set ("IdleCurrentA", DoubleValue (sleepPower));
  radioEnergyModelHelper.Install (devices, sources);

  Ptr<EnergySource> source0 = sources.Get (0);
  Ptr<EnergySource> source1 = sources.Get (1);

  Config::Connect ("/NodeList/0/$ns3::EnergySource/DeviceStatus", MakeCallback (&DeviceSleep));
  Config::Connect ("/NodeList/1/$ns3::EnergySource/DeviceStatus", MakeCallback (&DeviceSleep));

  if (tracing)
    {
      Simulator::TraceConfigSetDefault (" জলেরNodeName", StringValue ("/NodeList/*"));
      Simulator::TraceConfigSetDefault (" জলেরComponentAttribute", StringValue ("CurrentA"));
      Simulator::TraceConfigSetDefault (" জলেরEnableAscii", BooleanValue (true));
      Simulator::TraceConfigSetDefault (" জলেরAsciiFileName", StringValue ("energy-trace.txt"));
      Simulator::TraceConfigSetDefault (" জলেরEnablePcap", BooleanValue (true));
      Simulator::Simulator::EnableAsciiAll (true);
      wifiPhy.EnablePcapAll("wifi-simple-adhoc");
    }

  Simulator::Stop (Seconds (12.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}