#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

class EnergyLogger : public Object
{
public:
    EnergyLogger(Ptr<WifiRadioEnergyModel> wifiModel, Ptr<BasicEnergySource> energySource, Ptr<BasicEnergyHarvester> harvester) 
        : m_wifiModel(wifiModel), m_energySource(energySource), m_harvester(harvester) {}

    void Start()
    {
        LogStatistics();
    }

    void LogStatistics()
    {
        double residual = m_energySource->GetRemainingEnergy();
        double consumed = m_wifiModel->GetTotalEnergyConsumption();
        double harvested = m_harvester->GetTotalEnergyHarvested();
        double lastHarvestPower = m_harvester->GetHarvestedPower();
        NS_LOG_UNCOND("At " << Simulator::Now().GetSeconds() << "s:"
            << " Residual=" << residual << "J"
            << " Consumed=" << consumed << "J"
            << " Harvested=" << harvested << "J"
            << " CurrentHarvestPower=" << lastHarvestPower << "W");
        Simulator::Schedule(Seconds(1.0), &EnergyLogger::LogStatistics, this);
    }

    void PrintFinal()
    {
        double residual = m_energySource->GetRemainingEnergy();
        double consumed = m_wifiModel->GetTotalEnergyConsumption();
        double harvested = m_harvester->GetTotalEnergyHarvested();
        NS_LOG_UNCOND("=== Final Receiver Energy Statistics ===");
        NS_LOG_UNCOND("Residual energy: " << residual << " J");
        NS_LOG_UNCOND("Total energy consumed: " << consumed << " J");
        NS_LOG_UNCOND("Total energy harvested: " << harvested << " J");
    }

private:
    Ptr<WifiRadioEnergyModel> m_wifiModel;
    Ptr<BasicEnergySource> m_energySource;
    Ptr<BasicEnergyHarvester> m_harvester;
};

int main(int argc, char *argv[])
{
    LogComponentEnable("EnergyEfficientWirelessSimulation", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2); // 0: sender, 1: receiver

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(1));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(0));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    NetDeviceContainer devices;
    devices.Add(apDevices.Get(0));
    devices.Add(staDevices.Get(0));
    interfaces = address.Assign(devices);

    // Energy model setup
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0));
    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper wifiRadioEnergyHelper;
    wifiRadioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    wifiRadioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));

    // Attach the energy source to wifi device
    DeviceEnergyModelContainer deviceModels;
    Ptr<WifiNetDevice> txDev = DynamicCast<WifiNetDevice>(apDevices.Get(0));
    Ptr<WifiNetDevice> rxDev = DynamicCast<WifiNetDevice>(staDevices.Get(0));
    deviceModels.Add(wifiRadioEnergyHelper.Install(txDev, sources.Get(0)));
    deviceModels.Add(wifiRadioEnergyHelper.Install(rxDev, sources.Get(1)));

    // Energy Harvesting setup: Each node gets a harvester
    Ptr<UniformRandomVariable> uniform = CreateObject<UniformRandomVariable>();
    BasicEnergyHarvesterHelper harvesterHelper;
    harvesterHelper.Set("PeriodicHarvestedPowerUpdateInterval", TimeValue(Seconds(1.0)));
    harvesterHelper.Set("HarvestablePower", DoubleValue(0.0)); // Will be randomized
    EnergyHarvesterContainer harvesters;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<BasicEnergyHarvester> harv = DynamicCast<BasicEnergyHarvester>(harvesterHelper.Install(sources.Get(i)).Get(0));
        harvesters.Add(harv);
    }

    // Schedule updates to harvester power: each second, set between 0 and 0.1W
    for (uint32_t nodeIdx = 0; nodeIdx < 2; ++nodeIdx)
    {
        Ptr<BasicEnergyHarvester> harv = DynamicCast<BasicEnergyHarvester>(harvesters.Get(nodeIdx));
        for (int i = 0; i < 11; ++i)
        {
            Simulator::Schedule(Seconds(i),
                [harv, uniform]()
                {
                    double pwr = uniform->GetValue(0.0, 0.1);
                    harv->SetHarvestablePower(pwr);
                }
            );
        }
    }

    // UDP traffic: transmitter sends one packet per second for 10 seconds (packet duration 0.0023s)
    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(11.0));

    uint32_t pktSize = 1472; // Use max size for WiFi (or calculate based on data rate/tx time)
    double txTime = 0.0023; // seconds
    // For e.g., at 5Mbps, pktSize = 0.0023 x 5,000,000 / 8 = ~1437 bytes
    ApplicationContainer clientApps;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(pktSize * 8 / txTime))); // bits/sec for given pkt size
    onoff.SetAttribute("PacketSize", UintegerValue(pktSize));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0023]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.9977]"));

    clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(11.0));

    // Attach energy logger to receiver (node 1)
    Ptr<BasicEnergySource> rxSource = DynamicCast<BasicEnergySource>(sources.Get(1));
    Ptr<WifiRadioEnergyModel> rxRadio = DynamicCast<WifiRadioEnergyModel>(rxDev->GetObject<DeviceEnergyModelContainer>()->Get(0));
    Ptr<BasicEnergyHarvester> rxHarvester = DynamicCast<BasicEnergyHarvester>(harvesters.Get(1));
    Ptr<EnergyLogger> logger = CreateObject<EnergyLogger>(rxRadio, rxSource, rxHarvester);
    Simulator::Schedule(Seconds(0.0), &EnergyLogger::Start, logger);

    Simulator::Stop(Seconds(11.1));
    Simulator::Run();

    logger->PrintFinal();

    Simulator::Destroy();
    return 0;
}