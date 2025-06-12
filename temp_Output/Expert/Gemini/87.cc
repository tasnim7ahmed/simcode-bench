#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

class DataCollector {
public:
    DataCollector(std::string format, std::string runId) : format_(format), runId_(runId) {}

    void PacketTx(std::string context, Ptr<const Packet> packet) {
        txPackets++;
    }

    void PacketRx(std::string context, Ptr<const Packet> packet) {
        rxPackets++;
        Time arrivalTime = Simulator::Now();
        Ipv4Header ipHeader;
        packet->PeekHeader(ipHeader);
        Ipv4Address sourceAddress = ipHeader.GetSource();
        Ipv4Address destinationAddress = ipHeader.GetDestination();

        for (auto const& [seq, sendTime] : sentPacketTimes) {
            if (seq == packet->GetUid()) {
                Time delay = arrivalTime - sendTime;
                totalDelay += delay;
                sentPacketTimes.erase(seq);
                break;
            }
        }
    }

    void PacketSend(Ptr<const Packet> packet, Ipv4Address dest, uint16_t port) {
        seq++;
        sentPacketTimes[seq] = Simulator::Now();
    }

    void Finalize() {
        std::stringstream ss;
        if (format_ == "omnet") {
            ss << "scalar TxPackets " << txPackets << std::endl;
            ss << "scalar RxPackets " << rxPackets << std::endl;
            if (rxPackets > 0) {
                ss << "scalar AverageDelay " << totalDelay.GetSeconds() / rxPackets << std::endl;
            } else {
                ss << "scalar AverageDelay 0" << std::endl;
            }
            std::ofstream outfile(runId_ + ".sca");
            outfile << ss.str();
            outfile.close();
        } else if (format_ == "sqlite") {
            std::cout << "SQLite output not implemented yet." << std::endl;
        } else {
            std::cout << "Invalid output format." << std::endl;
        }
    }

    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    Time totalDelay = Seconds(0.0);
    std::map<uint32_t, Time> sentPacketTimes;
    uint32_t seq = 0;

private:
    std::string format_;
    std::string runId_;
};

int main(int argc, char* argv[]) {
    double distance = 10.0;
    std::string format = "omnet";
    std::string runId = "default";

    CommandLine cmd;
    cmd.AddValue("distance", "Distance between nodes (m)", distance);
    cmd.AddValue("format", "Output format (omnet, sqlite)", format);
    cmd.AddValue("runId", "Run identifier", runId);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distance),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(1),
        "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    DataCollector dataCollector(format, runId);

    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/$ns3::WifiNetDevice/Mac/MacTx", MakeCallback(&DataCollector::PacketTx, &dataCollector));
        Config::ConnectWithoutContext("/NodeList/" + std::to_string(i) + "/$ns3::WifiNetDevice/Mac/MacRx", MakeCallback(&DataCollector::PacketRx, &dataCollector));
    }
    echoClient.SetPacketSendCallback(MakeCallback(&DataCollector::PacketSend, &dataCollector));

    Simulator::Stop(Seconds(11.0));

    Simulator::Run();

    dataCollector.Finalize();

    Simulator::Destroy();
    return 0;
}