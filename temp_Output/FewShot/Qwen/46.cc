#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/command-line.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdhocEnergyExample");

void RemainingEnergyTrace(double oldValue, double remainingEnergy) {
    NS_LOG_INFO("Remaining energy = " << remainingEnergy << " J");
}

void StateChangeNotification(EnergySourceContainer sources) {
    for (uint32_t i = 0; i < sources.GetN(); ++i) {
        Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(i));
        if (source && source->GetRemainingEnergy() <= 0.0) {
            NS_LOG_UNCOND("Node " << i << " has entered sleep state due to energy depletion.");
        }
    }
}

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    uint32_t packetSize = 1000;
    double dataRate = 1e6; // in bps
    double txPowerLevel = 16.02; // dBm
    double initialEnergy = 1000.0; // Joules
    double supplyVoltage = 3.0; // Volts

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Data rate in bps", dataRate);
    cmd.AddValue("packetSize", "Size of application packets", packetSize);
    cmd.AddValue("txPowerLevel", "Transmission power level (dBm)", txPowerLevel);
    cmd.AddValue("initialEnergy", "Initial energy in joules", initialEnergy);
    cmd.AddValue("supplyVoltage", "Supply voltage in volts", supplyVoltage);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
    LogComponentEnable("BasicEnergyModel", LOG_LEVEL_ALL);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 9));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = onoff.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer serverApp = sink.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerStart", DoubleValue(txPowerLevel));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/TxPowerEnd", DoubleValue(txPowerLevel));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Number Of Antennas", UintegerValue(1));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/Nss", UintegerValue(1));

    EnergySourceContainer sources;
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(supplyVoltage));
    sources.Add(basicSourceHelper.Install(nodes));

    DeviceEnergyModelContainer deviceModels;
    WifiRadioEnergyModelHelper radioEnergyHelper;
    deviceModels.Add(radioEnergyHelper.Install(devices, sources));

    Config::ConnectWithoutContext("/NodeList/*/EnergySourceList/0/RemainingEnergy", MakeCallback(&RemainingEnergyTrace));
    sources.Connect(&StateChangeNotification);

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("remaining-energy-trace.txt");
    sources.Get(0)->TraceConnectWithoutContext("RemainingEnergy", MakeBoundCallback(&RemainingEnergyTrace));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}