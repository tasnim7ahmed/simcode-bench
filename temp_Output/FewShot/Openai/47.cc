#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include <fstream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAxSpatialReuseExample");

void StaResetEventLogger(std::string context, uint64_t oldValue, uint64_t newValue) {
    std::string staName;
    if (context.find("sta1") != std::string::npos)
        staName = "sta1";
    else if (context.find("sta2") != std::string::npos)
        staName = "sta2";
    else
        staName = "unknown_sta";

    static std::ofstream f1("sta1_spatial_reuse_resets.txt", std::ios::app);
    static std::ofstream f2("sta2_spatial_reuse_resets.txt", std::ios::app);

    std::ofstream* fout = nullptr;
    if (staName == "sta1") fout = &f1;
    else if (staName == "sta2") fout = &f2;

    if (fout)
        *fout << Simulator::Now().GetSeconds()
              << ": Reset events changed from " << oldValue << " to "
              << newValue << std::endl;
}

struct ThroughputCounter {
    uint64_t bytes;
    Ptr<OutputStreamWrapper> stream;

    ThroughputCounter() : bytes(0), stream(nullptr) {}

    void RxCallback(Ptr<const Packet> pkt, const Address& addr) {
        bytes += pkt->GetSize();
    }
};

int main(int argc, char* argv[]) {
    // ----------------------------- Parameters -----------------------------
    // Distances
    double d1 = 2.0;  // STA1 <-> AP1 (meters)
    double d2 = 2.0;  // STA2 <-> AP2 (meters)
    double d3 = 5.0;  // AP1 <-> AP2 (meters)
    // Transmit Power, Thresholds, ChannelWidth, etc.
    double txPowerDbm = 21.0;
    double ccaEdThresholdDbm = -62.0; // dBm
    double obssPdThresholdDbm = -72.0; // dBm
    bool obssPdEnable = true;
    uint32_t channelWidth = 20; // MHz
    double simTime = 10.0; // seconds

    // Traffic
    double trafficInterval = 0.001; // sec (1 ms)
    uint32_t packetSize = 1024; // bytes
    std::string phyMode = "HE_MCS9_HE20"; // updated depending on channelWidth

    // Parse from cmd line
    CommandLine cmd(__FILE__);
    cmd.AddValue("d1", "Distance between STA1 and AP1 (meters)", d1);
    cmd.AddValue("d2", "Distance between STA2 and AP2 (meters)", d2);
    cmd.AddValue("d3", "Distance between AP1 and AP2 (meters)", d3);
    cmd.AddValue("txPowerDbm", "Transmit Power (dBm)", txPowerDbm);
    cmd.AddValue("ccaEdThresholdDbm", "CCA-ED Threshold (dBm)", ccaEdThresholdDbm);
    cmd.AddValue("obssPdThresholdDbm", "OBSS-PD Threshold (dBm)", obssPdThresholdDbm);
    cmd.AddValue("obssPdEnable", "Enable OBSS-PD spatial reuse", obssPdEnable);
    cmd.AddValue("channelWidth", "Channel width (MHz, 20/40/80)", channelWidth);
    cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
    cmd.AddValue("trafficInterval", "Inter-packet interval (s)", trafficInterval);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.Parse(argc, argv);

    if (channelWidth == 20) phyMode = "HE_MCS9_HE20";
    else if (channelWidth == 40) phyMode = "HE_MCS9_HE40";
    else if (channelWidth == 80) phyMode = "HE_MCS9_HE80";
    else phyMode = "HE_MCS9_HE20"; // default

    // ----------------------------- Nodes -----------------------------
    NodeContainer apNodes, staNodes;
    apNodes.Create(2);
    staNodes.Create(2);

    // ----------------------------- Wifi 6 Setup -----------------------------
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(phyMode),
                                "ControlMode", StringValue(phyMode));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("TxPowerStart", DoubleValue(txPowerDbm));
    phy.Set("TxPowerEnd", DoubleValue(txPowerDbm));
    phy.Set("EnergyDetectionThreshold", DoubleValue(ccaEdThresholdDbm));
    phy.Set("CcaMode1Threshold", DoubleValue(ccaEdThresholdDbm));

    WifiMacHelper mac;

    // Spatial reuse (OBSS-PD) config
    // Per station basis using HE Wi-Fi spatial reuse attributes
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    // SSID/BSSIDs
    Ssid ssid1 = Ssid("bss1");
    Ssid ssid2 = Ssid("bss2");

    // AP1
    NetDeviceContainer apDevices1, staDevices1;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid1));
    apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    staDevices1 = wifi.Install(phy, mac, staNodes.Get(0));

    // AP2
    NetDeviceContainer apDevices2, staDevices2;
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid2));
    apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    staDevices2 = wifi.Install(phy, mac, staNodes.Get(1));

    // Set OBSS-PD and SpatialReuse support for STAs
    // (applies only if OBSS-PD spatial reuse is enabled)
    if (obssPdEnable) {
        Config::Set("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdThreshold",
                    DoubleValue(obssPdThresholdDbm)); // STA1
        Config::Set("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdThreshold",
                    DoubleValue(obssPdThresholdDbm)); // STA2
        Config::Set("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdEnable",
                    BooleanValue(true)); // STA1
        Config::Set("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdEnable",
                    BooleanValue(true)); // STA2
    } else {
        Config::Set("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdEnable",
                    BooleanValue(false));
        Config::Set("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ObssPdEnable",
                    BooleanValue(false));
    }

    // ----------------------------- Mobility -----------------------------
    MobilityHelper mobility;

    // AP1 at (0,0,0)
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    // AP2 at (d3,0,0)
    positionAlloc->Add(Vector(d3, 0.0, 0.0));
    // STA1 at (0,d1,0)
    positionAlloc->Add(Vector(0.0, d1, 0.0));
    // STA2 at (d3,d2,0)
    positionAlloc->Add(Vector(d3, d2, 0.0));

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer(apNodes, staNodes));

    // ----------------------------- Internet stack -----------------------------
    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // Assign IP addresses
    Ipv4AddressHelper address1, address2;
    address1.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceApSta1;
    ifaceApSta1.Add(address1.Assign(apDevices1));
    ifaceApSta1.Add(address1.Assign(staDevices1));

    address2.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ifaceApSta2;
    ifaceApSta2.Add(address2.Assign(apDevices2));
    ifaceApSta2.Add(address2.Assign(staDevices2));

    // ----------------------------- Applications (traffic) -----------------------------
    // Use UDP OnOff applications to simulate saturated traffic

    // STA1 -> AP1
    uint16_t port1 = 6000;
    OnOffHelper onoff1("ns3::UdpSocketFactory", 
                       InetSocketAddress(ifaceApSta1.GetAddress(0), port1)); // AP1 address
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff1.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff1.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(uint32_t(double(packetSize*8)/trafficInterval)) + "bps")));

    ApplicationContainer sta1App = onoff1.Install(staNodes.Get(0));
    sta1App.Start(Seconds(1.0));
    sta1App.Stop(Seconds(simTime));

    PacketSinkHelper sink1("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port1));
    ApplicationContainer ap1App = sink1.Install(apNodes.Get(0));
    ap1App.Start(Seconds(0.0));
    ap1App.Stop(Seconds(simTime));

    // STA2 -> AP2
    uint16_t port2 = 7000;
    OnOffHelper onoff2("ns3::UdpSocketFactory", 
                       InetSocketAddress(ifaceApSta2.GetAddress(0), port2)); // AP2 address
    onoff2.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff2.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff2.SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(uint32_t(double(packetSize*8)/trafficInterval)) + "bps")));

    ApplicationContainer sta2App = onoff2.Install(staNodes.Get(1));
    sta2App.Start(Seconds(1.0));
    sta2App.Stop(Seconds(simTime));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port2));
    ApplicationContainer ap2App = sink2.Install(apNodes.Get(1));
    ap2App.Start(Seconds(0.0));
    ap2App.Stop(Seconds(simTime));

    // ---------------------------- Logging spatial reuse reset events ----------------------------
    Config::Connect("/NodeList/2/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ResetEvents",
                    MakeBoundCallback(&StaResetEventLogger, "sta1"));
    Config::Connect("/NodeList/3/DeviceList/0/$ns3::WifiNetDevice/Phy/HeObssPd/ResetEvents",
                    MakeBoundCallback(&StaResetEventLogger, "sta2"));

    // ----------------------------- Throughput measurement -----------------------------
    ThroughputCounter th1, th2;

    Ptr<PacketSink> sinkApp1 = DynamicCast<PacketSink>(ap1App.Get(0));
    Ptr<PacketSink> sinkApp2 = DynamicCast<PacketSink>(ap2App.Get(0));
    sinkApp1->TraceConnectWithoutContext("Rx", MakeCallback(&ThroughputCounter::RxCallback, &th1));
    sinkApp2->TraceConnectWithoutContext("Rx", MakeCallback(&ThroughputCounter::RxCallback, &th2));

    // ----------------------------- Enable Logging -----------------------------
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // ----------------------------- Channel effects -----------------------------
    // (Already handled by setting channel width, tx power, sensitivity, distances)

    // Enable PCAP if you want to debug (uncomment)
    // phy.EnablePcapAll("wifi-ax-spatial-reuse");

    // ----------------------------- Run Simulation -----------------------------
    Simulator::Stop(Seconds(simTime + 0.1));
    Simulator::Run();

    // ----------------------------- Output Throughput Results -----------------------------
    double thr1 = (double)th1.bytes * 8 / (simTime - 1.0) / 1e6; // Mbps, starts at 1.0
    double thr2 = (double)th2.bytes * 8 / (simTime - 1.0) / 1e6; // Mbps

    std::cout << "================== Simulation Results ==================" << std::endl;
    std::cout << "BSS1 Throughput (STA1->AP1): " << thr1 << " Mbps" << std::endl;
    std::cout << "BSS2 Throughput (STA2->AP2): " << thr2 << " Mbps" << std::endl;
    std::cout << "OBSS-PD Spatial Reuse: " << (obssPdEnable ? "ENABLED" : "DISABLED") << std::endl;
    std::cout << "Channel Width: " << channelWidth << " MHz" << std::endl;
    std::cout << "d1=" << d1 << "m, d2=" << d2 << "m, d3=" << d3 << "m" << std::endl;
    std::cout << "========================================================" << std::endl;

    Simulator::Destroy();
    return 0;
}