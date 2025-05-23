#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    double rxPowerDbM = -60.0;
    double txPowerDbM = 17.0;
    uint32_t packetSize = 1000;
    uint32_t numPackets = 1;
    double simulationTime = 5.0;
    bool verbose = false;
    bool pcap = false;
    std::string dataRate = "1Mbps";
    double energyDetectionThresholdDbM = -82.0;
    double ccaMode1ThresholdDbM = -79.0;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("rxPower", "Target received signal strength in dBm (used to calculate propagation loss).", rxPowerDbM);
    cmd.AddValue("txPower", "Transmit power in dBm.", txPowerDbM);
    cmd.AddValue("packetSize", "Size of the UDP packet in bytes.", packetSize);
    cmd.AddValue("numPackets", "Number of UDP packets to send.", numPackets);
    cmd.AddValue("simTime", "Total simulation time in seconds.", simulationTime);
    cmd.AddValue("verbose", "Enable verbose logging.", verbose);
    cmd.AddValue("pcap", "Enable PCAP tracing.", pcap);
    cmd.AddValue("dataRate", "Wifi data rate (e.g., 1Mbps, 2Mbps, 5.5Mbps, 11Mbps for 802.11b).", dataRate);
    cmd.AddValue("energyDetectionThreshold", "PHY EnergyDetectionThreshold in dBm.", energyDetectionThresholdDbM);
    cmd.AddValue("ccaThreshold", "PHY CcaMode1Threshold in dBm.", ccaMode1ThresholdDbM);
    cmd.Parse(argc, argv);

    if (verbose)
    {
        ns3::LogComponentEnable("WifiFixedRss", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("OnOffApplication", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
        ns3::LogComponentEnable("YansWifiPhy", ns3::LOG_LEVEL_INFO);
    }

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    ns3::YansWifiChannelHelper channel;
    double propagationLoss = txPowerDbM - rxPowerDbM;
    channel.SetPropagationLossModel("ns3::ConstantPropagationLossModel", "Loss", ns3::DoubleValue(propagationLoss));
    channel.SetPropagationDelayModel("ns3::ConstantPropagationDelayModel");

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPower", ns3::DoubleValue(txPowerDbM));
    phy.Set("EnergyDetectionThreshold", ns3::DoubleValue(energyDetectionThresholdDbM));
    phy.Set("CcaMode1Threshold", ns3::DoubleValue(ccaMode1ThresholdDbM));

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::AdhocWifiMacHelper adhocMac;

    ns3::NetDeviceContainer devices;
    devices = wifi.Install(phy, adhocMac, nodes);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), 9));
    ns3::ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(ns3::Seconds(0.0));
    sinkApp.Stop(ns3::Seconds(simulationTime));

    ns3::OnOffHelper onoff("ns3::UdpSocketFactory",
                           ns3::InetSocketAddress(interfaces.GetAddress(1), 9));
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate(dataRate)));
    onoff.SetAttribute("MaxBytes", ns3::UintegerValue(numPackets * packetSize));

    ns3::ApplicationContainer onoffApp = onoff.Install(nodes.Get(0));
    onoffApp.Start(ns3::Seconds(1.0));
    onoffApp.Stop(ns3::Seconds(simulationTime));

    if (pcap)
    {
        phy.EnablePcapAll("wifi-fixed-rss");
    }

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}