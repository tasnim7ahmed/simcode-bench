#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include <fstream>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("EnergyEfficientWireless");

class EnergyLogger
{
public:
  EnergyLogger (Ptr<BasicEnergySource> energySource, Ptr<EnergyHarvester> harvester)
    : m_energySource (energySource),
      m_harvester (harvester)
  {
    std::ofstream log ("receiver-energy.log");
    log << "Time(s),ResidualEnergy(J),HarvestedPower(W),TotalEnergyConsumed(J)\n";
    log.close ();
  }

  void Log ()
  {
    double time = Simulator::Now ().GetSeconds ();
    double residual = m_energySource->GetRemainingEnergy ();
    double harvested = m_harvester->GetPower ()->GetValue ();
    double consumed = m_energySource->GetInitialEnergy () - residual;
    std::ofstream log ("receiver-energy.log", std::ios_base::app);
    log << std::fixed << std::setprecision (6)
        << time << ","
        << residual << ","
        << harvested << ","
        << consumed << "\n";
    log.close ();
    Simulator::Schedule (Seconds (1.0), &EnergyLogger::Log, this);
  }

  void PrintSummary ()
  {
    double residual = m_energySource->GetRemainingEnergy ();
    double harvestedTotal = 0;
    double consumed = m_energySource->GetInitialEnergy () - residual;

    // Sum harvested energy throughout the log file
    std::ifstream log ("receiver-energy.log");
    std::string line;
    getline (log, line); // skip header
    while (getline (log, line))
      {
        std::stringstream ss (line);
        std::string item;
        getline (ss, item, ','); // time
        getline (ss, item, ','); // residual
        getline (ss, item, ',');
        harvestedTotal += std::stod (item);
      }
    harvestedTotal = harvestedTotal; // Already per second snapshot, so integral is approximation

    std::cout << "=== Receiver Energy Statistics ===" << std::endl;
    std::cout << "Initial energy: " << m_energySource->GetInitialEnergy () << " J\n";
    std::cout << "Final residual energy: " << residual << " J\n";
    std::cout << "Total consumed energy: " << consumed << " J\n";
    std::cout << "Total harvested power samples sum: " << harvestedTotal << " W (sum of samples, not integrated)\n";
  }

private:
  Ptr<BasicEnergySource> m_energySource;
  Ptr<EnergyHarvester> m_harvester;
};

int main (int argc, char *argv[])
{
  // Set simulation time
  double simTime = 10.0;

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Position nodes
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                "MinX", DoubleValue (0.0),
                                "MinY", DoubleValue (0.0),
                                "DeltaX", DoubleValue (10.0),
                                "DeltaY", DoubleValue (0.0),
                                "GridWidth", UintegerValue (2),
                                "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // WiFi network
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211g);
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  phy.SetChannel (channel.Create ());
  WifiMacHelper mac;
  Ssid ssid = Ssid ("energy-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, nodes.Get (1));
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, nodes.Get (0));
  NetDeviceContainer devices = NetDeviceContainer (apDevice.Get (0), staDevice.Get (0));

  // Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // Energy sources and harvesters
  BasicEnergySourceHelper basicSourceHelper;
  basicSourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (1.0));
  EnergySourceContainer sources = basicSourceHelper.Install (nodes);

  // WiFi radio energy model
  WifiRadioEnergyModelHelper radioEnergyHelper;
  // For both nodes
  radioEnergyHelper.Set ("TxCurrentA", DoubleValue (0.0174));
  radioEnergyHelper.Set ("RxCurrentA", DoubleValue (0.0197));
  DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);

  // Harvesters (random power 0..0.1 W, updated every 1s)
  Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
  uv->SetAttribute ("Min", DoubleValue (0.0));
  uv->SetAttribute ("Max", DoubleValue (0.1));
  BasicEnergyHarvesterHelper harvesterHelper;
  harvesterHelper.Set ("PeriodicHarvestedPowerUpdateInterval", TimeValue (Seconds (1.0)));
  harvesterHelper.Set ("HarvestablePower", DoubleValue (uv->GetValue ()));
  harvesterHelper.Set ("RandomHarvestablePower", PointerValue (uv));
  EnergyHarvesterContainer harvesters = harvesterHelper.Install (sources);

  // Application: Sender (node 0) -> Receiver (node 1)
  uint16_t port = 9;
  // Install UDP server on receiver
  UdpServerHelper server (port);
  ApplicationContainer serverApp = server.Install (nodes.Get (1));
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // Install UDP client on sender
  UdpClientHelper client (interfaces.GetAddress (1), port);
  client.SetAttribute ("MaxPackets", UintegerValue (10));
  client.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  client.SetAttribute ("PacketSize", UintegerValue (1472)); // Default maximum Ethernet payload

  ApplicationContainer clientApp = client.Install (nodes.Get (0));
  clientApp.Start (Seconds (0.0));
  clientApp.Stop (Seconds (simTime));

  // Set constant PHY rate and transmission duration (can tweak MCS and payload to get 0.0023s tx time)
  WifiPhyHelper::SetPcapDataLinkType (phy, WifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue ("ErpOfdmRate54Mbps"),
                               "ControlMode", StringValue ("ErpOfdmRate54Mbps"));

  // Energy logger for receiver node
  Ptr<BasicEnergySource> recvEnergySource = DynamicCast<BasicEnergySource> (sources.Get (1));
  Ptr<EnergyHarvester> recvHarvester = harvesters.Get (1);
  Ptr<WifiRadioEnergyModel> recvWifiModel = DynamicCast<WifiRadioEnergyModel> (deviceModels.Get (1));
  Ptr<EnergyLogger> logger = CreateObject<EnergyLogger> (recvEnergySource, recvHarvester);

  Simulator::Schedule (Seconds (0.0), &EnergyLogger::Log, logger);

  Simulator::Stop (Seconds (simTime + 0.1));
  Simulator::Run ();

  logger->PrintSummary ();

  Simulator::Destroy ();
  return 0;
}