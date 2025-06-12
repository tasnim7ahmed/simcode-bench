#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/spectrum-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiGoodputSimulation");

class SimulationRunner {
public:
  SimulationRunner();
  void Run();

private:
  void Setup(uint8_t mcs, uint16_t channelWidth, bool rtsCts, bool useTcp, double distance, WifiGuardInterval guardInterval);
  void ReportResults();
  void ReceivePacket(Ptr<const Packet> packet, const Address& from);

  uint32_t m_receivedBytes;
  std::ofstream m_outputFile;
};

SimulationRunner::SimulationRunner()
  : m_receivedBytes(0)
{
  m_outputFile.open("wifi_goodput.csv");
  m_outputFile << "MCS,ChannelWidth,RTS_CTS,TCP_UDP,Distance,GuardInterval,ThroughputMbps\n";
}

void
SimulationRunner::Run()
{
  for (uint8_t mcs = 0; mcs <= 9; ++mcs)
    {
      for (auto channelWidth : {20, 40, 80, 160})
        {
          for (auto rtsCts : {false, true})
            {
              for (auto useTcp : {false, true})
                {
                  for (double distance : {1.0, 5.0, 10.0, 20.0})
                    {
                      for (auto guardInterval : {WifiGuardInterval::SHORT, WifiGuardInterval::LONG})
                        {
                          NS_LOG_INFO("Running: MCS=" << +mcs << ", ChannelWidth=" << channelWidth << ", RTS/CTS=" << rtsCts
                                                      << ", Protocol=" << (useTcp ? "TCP" : "UDP") << ", Distance=" << distance
                                                      << ", GuardInterval=" << (guardInterval == WifiGuardInterval::SHORT ? "Short" : "Long"));
                          Setup(mcs, channelWidth, rtsCts, useTcp, distance, guardInterval);
                          Simulator::Run();
                          ReportResults();
                          Simulator::Destroy();
                        }
                    }
                }
            }
        }
    }

  m_outputFile.close();
}

void
SimulationRunner::Setup(uint8_t mcs, uint16_t channelWidth, bool rtsCts, bool useTcp, double distance, WifiGuardInterval guardInterval)
{
  m_receivedBytes = 0;

  NodeContainer wifiStaNode;
  wifiStaNode.Create(1);
  NodeContainer wifiApNode;
  wifiApNode.Create(1);

  YansWifiPhyHelper phy;
  phy.Set("ChannelSettings", StringValue("{0, 80+80}"));

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("VhtMcs" + std::to_string(mcs)),
                               "ControlMode", StringValue("VhtMcs" + std::to_string(mcs)));

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns-3-ssid");

  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "ActiveProbing", BooleanValue(false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install(phy, mac, wifiStaNode);

  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "BeaconGeneration", BooleanValue(true),
              "BeaconInterval", TimeValue(Seconds(2.5)));

  if (rtsCts)
    {
      wifi.SetRtsEpsilon(1500);
    }

  NetDeviceContainer apDevices;
  apDevices = wifi.Install(phy, mac, wifiApNode);

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(distance, 0.0, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNode);
  mobility.Install(wifiApNode);

  InternetStackHelper stack;
  stack.Install(wifiStaNode);
  stack.Install(wifiApNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");

  Ipv4InterfaceContainer staInterfaces;
  Ipv4InterfaceContainer apInterface;
  staInterfaces = address.Assign(staDevices);
  apInterface = address.Assign(apDevices);

  uint16_t port = 9;
  if (useTcp)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(wifiStaNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(10.0));

      OnOffHelper client("ns3::TcpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      client.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
      client.SetAttribute("PacketSize", UintegerValue(1472));

      ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(10.0));
    }
  else
    {
      PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
      ApplicationContainer sinkApp = sink.Install(wifiStaNode.Get(0));
      sinkApp.Start(Seconds(0.0));
      sinkApp.Stop(Seconds(10.0));

      OnOffHelper client("ns3::UdpSocketFactory", InetSocketAddress(staInterfaces.GetAddress(0), port));
      client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
      client.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
      client.SetAttribute("PacketSize", UintegerValue(1472));

      ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
      clientApp.Start(Seconds(1.0));
      clientApp.Stop(Seconds(10.0));

      Config::ConnectWithoutContext("/NodeList/0/ApplicationList/0/$ns3::PacketSink/Rx", MakeCallback(&SimulationRunner::ReceivePacket, this));
    }

  Simulator::Stop(Seconds(10.0));
}

void
SimulationRunner::ReportResults()
{
  double durationSeconds = 9.0;
  double throughputMbps = (m_receivedBytes * 8) / (durationSeconds * 1e6);
  m_outputFile << +m_receivedBytes << "," << m_receivedBytes << "," << throughputMbps << "\n";
  m_outputFile.flush();
}

void
SimulationRunner::ReceivePacket(Ptr<const Packet> packet, const Address& from)
{
  m_receivedBytes += packet->GetSize();
}

int main(int argc, char *argv[])
{
  LogComponentEnable("WifiGoodputSimulation", LOG_LEVEL_INFO);
  SimulationRunner runner;
  runner.Run();
  return 0;
}