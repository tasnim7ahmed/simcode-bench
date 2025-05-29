#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include <map>
#include <numeric>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiQoSAcExample");

struct Stats {
    uint64_t totalTxBytes = 0;
    uint64_t rxBytes = 0;
    std::vector<double> txopDurations;
};

std::map<uint32_t, Stats> acStats; // Key: NetworkIndex (0-3) + AC

void
RxPacketCounter(
    Ptr<const Packet> packet,
    const Address &,
    uint32_t networkIndex,
    uint32_t acId)
{
    acStats[networkIndex * 4 + acId].rxBytes += packet->GetSize();
}

void
TxPacketCounter(
    Ptr<const Packet> packet,
    uint32_t networkIndex,
    uint32_t acId)
{
    acStats[networkIndex * 4 + acId].totalTxBytes += packet->GetSize();
}

void
TxopTraceCb(
    std::string context,
    Time txopDuration,
    uint32_t networkIndex,
    uint32_t acId)
{
    acStats[networkIndex * 4 + acId].txopDurations.push_back(txopDuration.GetSeconds());
}

enum AccessCategory
{
    AC_BE = 0,
    AC_BK = 1,
    AC_VI = 2,
    AC_VO = 3
};

void
ConfigureOnOffApp(
    Ptr<Node> sender,
    Ipv4Address destAddr,
    uint16_t port,
    uint32_t payloadSize,
    std::string ac,
    double appStart,
    double appStop,
    uint32_t networkIndex,
    uint32_t acId,
    bool enableTrace)
{
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(destAddr, port));
    onoff.SetConstantRate(DataRate("20Mbps"), payloadSize);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(appStart)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(appStop)));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("20Mbps")));
    onoff.SetAttribute("MaxBytes", UintegerValue(0));

    // Set QoS TID mapping
    Config::Set("/NodeList/" + std::to_string(sender->GetId()) +
                    "/ApplicationList/*/$ns3::OnOffApplication/Tid",
                UintegerValue(acId));
    ApplicationContainer app = onoff.Install(sender);

    if (enableTrace)
    {
        app.Get(0)->TraceConnectWithoutContext(
            "Tx", MakeBoundCallback(&TxPacketCounter, networkIndex, acId));
    }
}

int
main(int argc, char *argv[])
{
    uint32_t payloadSize = 1024;
    double distance = 20.0;
    bool enableRtsCts = false;
    double simTime = 10.0;
    bool enablePcap = false;

    CommandLine cmd;
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payloadSize);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("rtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.Parse(argc, argv);

    // Four independent Wi-Fi networks (one STA + one AP each)
    static const uint32_t nNetworks = 4;
    uint16_t udpPort = 5000;

    NodeContainer stas, aps;
    stas.Create(nNetworks);
    aps.Create(nNetworks);

    YansWifiPhyHelper phy[nNetworks];
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    NetDeviceContainer staDevices[nNetworks], apDevices[nNetworks];

    InternetStackHelper stack;
    stack.Install(stas);
    stack.Install(aps);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nNetworks; ++i)
    {
        // Station position
        posAlloc->Add(Vector(i * distance * 2, 0.0, 0.0));
    }
    for (uint32_t i = 0; i < nNetworks; ++i)
    {
        // AP position
        posAlloc->Add(Vector(i * distance * 2 + distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    NodeContainer allNodes;
    allNodes.Add(stas);
    allNodes.Add(aps);
    mobility.Install(allNodes);

    std::vector<std::string> ssids = {"ssid-0", "ssid-1", "ssid-2", "ssid-3"};

    // Setup for each network.
    std::vector<Ipv4InterfaceContainer> staInterfaces(nNetworks), apInterfaces(nNetworks);
    Ipv4AddressHelper ipv4;
    std::vector<std::string> baseAddresses = { "10.0.0.0", "10.1.0.0", "10.2.0.0", "10.3.0.0" };

    for (uint32_t i = 0; i < nNetworks; ++i)
    {
        Ssid ssid = Ssid(ssids[i]);
        phy[i] = YansWifiPhyHelper::Default();
        phy[i].SetChannel(channel.Create());
        if (enablePcap)
        {
            phy[i].SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        }

        uint32_t channelNumber = 36 + i * 4;
        WifiMacHelper staMac, apMac;

        // STA MAC QoS
        staMac.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssid),
                       "ActiveProbing", BooleanValue(false),
                       "QosSupported", BooleanValue(true));

        // AP MAC QoS
        apMac.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssid),
                      "QosSupported", BooleanValue(true));

        // Set channel for STA and AP
        Config::SetDefault("ns3::WifiPhy::ChannelNumber", UintegerValue(channelNumber));
        // Use RTS/CTS if enabled
        if (enableRtsCts)
        {
            Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
        }
        else
        {
            Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(2347));
        }
        // QoS WifiManager
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue("HtMcs7"),
                                    "ControlMode", StringValue("HtMcs0"));

        staDevices[i] = wifi.Install(phy[i], staMac, stas.Get(i));
        apDevices[i] = wifi.Install(phy[i], apMac, aps.Get(i));

        if (enablePcap)
        {
            phy[i].EnablePcap("wifi_qosac_ap" + std::to_string(i),
                              apDevices[i], false);
            phy[i].EnablePcap("wifi_qosac_sta" + std::to_string(i),
                              staDevices[i], false);
        }

        // Assign IP addresses
        ipv4.SetBase(baseAddresses[i].c_str(), "255.255.255.0");
        staInterfaces[i] = ipv4.Assign(staDevices[i]);
        apInterfaces[i] = ipv4.Assign(apDevices[i]);
    }

    // Set up applications and statistics for each network and each AC
    ApplicationContainer serverApps[nNetworks][2]; // [network][0=BE,1=VI]
    double startTime = 1.0;

    for (uint32_t i = 0; i < nNetworks; ++i)
    {
        // AC_BE = 0, AC_VI = 2
        for (uint32_t j = 0; j < 2; ++j)
        {
            uint32_t acId = (j == 0) ? AC_BE : AC_VI;
            // Udp server on AP
            UdpServerHelper server(udpPort + acId);
            serverApps[i][j] = server.Install(aps.Get(i));
            serverApps[i][j].Start(Seconds(startTime));
            serverApps[i][j].Stop(Seconds(simTime));

            // Trace RX bytes at server
            serverApps[i][j].Get(0)->TraceConnectWithoutContext(
                "Rx", MakeBoundCallback(&RxPacketCounter, i, acId));

            // Udp OnOff app on STA for each AC
            ConfigureOnOffApp(
                stas.Get(i),
                apInterfaces[i].GetAddress(0),
                udpPort + acId,
                payloadSize,
                (acId == AC_BE) ? "AC_BE" : "AC_VI",
                startTime + 0.1 * j,
                simTime,
                i,
                acId,
                true);

            // TXOP tracing (per network/ac)
            std::ostringstream txopPath;
            txopPath << "/NodeList/" << stas.Get(i)->GetId()
                     << "/DeviceList/0/$ns3::WifiNetDevice/Mac/"
                        "TxopTrace";
            Config::Connect(
                txopPath.str(),
                MakeBoundCallback(&TxopTraceCb, i, acId));
        }
    }

    Simulator::Stop(Seconds(simTime + 0.5));
    Simulator::Run();

    // Print stats
    std::cout << "\n========= Results =========" << std::endl;
    double totalThroughput = 0.0;
    for (uint32_t i = 0; i < nNetworks; ++i)
    {
        for (uint32_t j = 0; j < 2; ++j)
        {
            uint32_t acId = (j == 0) ? AC_BE : AC_VI;
            Stats &stat = acStats[i * 4 + acId];

            double throughputMbps = stat.rxBytes * 8.0 / (simTime - startTime) / 1e6;
            totalThroughput += throughputMbps;

            std::cout << "Network " << i << " [" << ((acId==0)?"AC_BE":"AC_VI") << "] : ";
            std::cout << " Received=" << stat.rxBytes << " bytes, ";
            std::cout << "Throughput=" << throughputMbps << " Mbps, ";

            double avgTxop = 0.0;
            if (!stat.txopDurations.empty())
            {
                avgTxop = std::accumulate(stat.txopDurations.begin(),
                                          stat.txopDurations.end(), 0.0)
                          / stat.txopDurations.size();
            }
            std::cout << "TXOP samples=" << stat.txopDurations.size() << ", ";
            std::cout << "Avg TXOP=" << avgTxop << " s" << std::endl;
        }
    }

    std::cout << "Total throughput across all networks: " << totalThroughput << " Mbps" << std::endl;
    std::cout << "===========================\n";

    Simulator::Destroy();
    return 0;
}