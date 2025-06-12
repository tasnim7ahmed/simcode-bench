#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QosWi-FiNetwork");

class QosWifiSimulation
{
public:
  QosWifiSimulation();
  void Run(uint32_t payloadSize, double distance, bool enableRtsCts, Time simulationTime, bool pcapEnabled);

private:
  void SetupNodes();
  void SetupDevices(bool enableRtsCts);
  void SetupMobility(double distance);
  void SetupApplications();
  void SetupStack();
  void EnableTracing(bool pcapEnabled);
  void PrintResults();

  uint32_t m_payloadSize;
  Time m_simulationTime;
  NodeContainer apNodes[4];
  NodeContainer staNodes[4];
  NetDeviceContainer apDevices[4];
  NetDeviceContainer staDevices[4];
  Ipv4InterfaceContainer apInterfaces[4];
  Ipv4InterfaceContainer staInterfaces[4];
  UdpServerHelper m_serverHelper;
  ApplicationContainer m_servers;
  ApplicationContainer m_clients;
  std::vector<double> m_txopDurations;
  std::map<Mac48Address, Time> m_lastTxopStart;
};

QosWifiSimulation::QosWifiSimulation()
{
  m_serverHelper = UdpServerHelper();
}

void QosWifiSimulation::Run(uint32_t payloadSize, double distance, bool enableRtsCts, Time simulationTime, bool pcapEnabled)
{
  m_payloadSize = payloadSize;
  m_simulationTime = simulationTime;

  SetupNodes();
  SetupMobility(distance);
  SetupDevices(enableRtsCts);
  SetupStack();
  SetupApplications();
  EnableTracing(pcapEnabled);

  Simulator::Stop(simulationTime);
  Simulator::Run();
  PrintResults();
  Simulator::Destroy();
}

void QosWifiSimulation::SetupNodes()
{
  for (uint32_t i = 0; i < 4; ++i)
    {
      apNodes[i].Create(1);
      staNodes[i].Create(1);
    }
}

void QosWifiSimulation::SetupMobility(double distance)
{
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(distance),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(4),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(apNodes[0]);
  mobility.Install(staNodes[0]);

  for (uint32_t i = 1; i < 4; ++i)
    {
      mobility.Install(apNodes[i]);
      mobility.Install(staNodes[i]);
    }
}

void QosWifiSimulation::SetupDevices(bool enableRtsCts)
{
  WifiMacHelper wifiMac;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

  if (enableRtsCts)
    {
      wifi.SetRtsEpsilon(1500);
    }

  Ssid ssid;
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
  phy.SetChannel(channel.Create());

  for (uint32_t i = 0; i < 4; ++i)
    {
      ssid = Ssid("qos-network-" + std::to_string(i));
      wifiMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "QosSupported", BooleanValue(true));

      apDevices[i] = wifi.Install(phy, wifiMac, apNodes[i]);

      wifiMac.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssid),
                      "ActiveProbing", BooleanValue(false),
                      "QosSupported", BooleanValue(true));

      staDevices[i] = wifi.Install(phy, wifiMac, staNodes[i]);

      // Configure QoS TXOP for AC_BE and AC_VI
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/Txop/Queue/MaxSize", StringValue("100p"));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/ViTxop/Queue/MaxSize", StringValue("100p"));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/VoTxop/Queue/MaxSize", StringValue("100p"));
      Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BkTxop/Queue/MaxSize", StringValue("100p"));

      // Trace TXOP duration
      phy.GetPhy(0)->TraceConnectWithoutContext("TxopDuration", MakeCallback(
        [this](Time duration) {
          m_txopDurations.push_back(duration.GetSeconds());
        }));
    }
}

void QosWifiSimulation::SetupStack()
{
  InternetStackHelper stack;
  for (uint32_t i = 0; i < 4; ++i)
    {
      stack.Install(apNodes[i]);
      stack.Install(staNodes[i]);
    }

  Ipv4AddressHelper address;
  for (uint32_t i = 0; i < 4; ++i)
    {
      std::ostringstream subnet;
      subnet << "192.168." << i << ".0";
      address.SetBase(subnet.str().c_str(), "255.255.255.0");
      apInterfaces[i] = address.Assign(apDevices[i]);
      staInterfaces[i] = address.Assign(staDevices[i]);
    }
}

void QosWifiSimulation::SetupApplications()
{
  for (uint32_t i = 0; i < 4; ++i)
    {
      m_servers.Add(m_serverHelper.Install(apNodes[i].Get(0)));
      m_servers.Get(i)->SetStartTime(Seconds(0.0));
      m_servers.Get(i)->SetStopTime(m_simulationTime);

      UdpClientHelper client(staInterfaces[i].GetAddress(0), 49153);
      client.SetAttribute("MaxPackets", UintegerValue(1000000));
      client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
      client.SetAttribute("PacketSize", UintegerValue(m_payloadSize));

      OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces[i].GetAddress(0), 49153));
      onoff.SetConstantRate(DataRate("1Mbps"));
      onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
      onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

      ApplicationContainer clientApp = onoff.Install(staNodes[i].Get(0));
      clientApp.Start(Seconds(0.1));
      clientApp.Stop(m_simulationTime - Seconds(0.1));
    }
}

void QosWifiSimulation::EnableTracing(bool pcapEnabled)
{
  if (pcapEnabled)
    {
      for (uint32_t i = 0; i < 4; ++i)
        {
          AsciiTraceHelper asciiTraceHelper;
          Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("qos-wifi-" + std::to_string(i) + ".tr");
          phy.EnableAsciiAll(stream);
          phy.EnablePcapAll("qos-wifi-" + std::to_string(i));
        }
    }
}

void QosWifiSimulation::PrintResults()
{
  std::cout.precision(4);
  std::cout.setf(std::ios::fixed);

  std::cout << "\nThroughput and TXOP Duration Results:" << std::endl;
  for (uint32_t i = 0; i < 4; ++i)
    {
      UdpServer server = dynamic_cast<UdpServer&>(*m_servers.Get(i));
      double totalBytes = server.GetReceived() * m_payloadSize;
      double throughput = totalBytes / (m_simulationTime.GetSeconds()) / 1e6; // Mbps

      std::cout << "Network " << i << ": Throughput = " << throughput << " Mbps" << std::endl;
    }

  double avgTxop = 0;
  if (!m_txopDurations.empty())
    {
      avgTxop = std::accumulate(m_txopDurations.begin(), m_txopDurations.end(), 0.0) / m_txopDurations.size();
    }
  std::cout << "Average TXOP Duration: " << avgTxop * 1e3 << " ms" << std::endl;
}

int main(int argc, char *argv[])
{
  uint32_t payloadSize = 1024;
  double distance = 10.0;
  bool enableRtsCts = false;
  Time simulationTime = Seconds(10);
  bool pcapEnabled = false;

  CommandLine cmd(__FILE__);
  cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
  cmd.AddValue("enableRtsCts", "Enable RTS/CTS handshake", enableRtsCts);
  cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
  cmd.AddValue("pcap", "Enable PCAP tracing", pcapEnabled);
  cmd.Parse(argc, argv);

  QosWifiSimulation sim;
  sim.Run(payloadSize, distance, enableRtsCts, simulationTime, pcapEnabled);

  return 0;
}