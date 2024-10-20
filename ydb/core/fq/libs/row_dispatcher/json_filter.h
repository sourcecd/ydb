#pragma once

#include <ydb/library/yql/public/udf/udf_data_type.h>
#include <ydb/library/yql/public/udf/udf_value.h>

namespace NFq {

class TJsonFilter {
public:
    using TCallback = std::function<void(ui64, const TString&)>;

public:
    TJsonFilter(
        const TVector<TString>& columns,
        const TVector<TString>& types,
        const TString& whereFilter,
        TCallback callback);

    ~TJsonFilter();

    void Push(const TVector<ui64>& offsets, const TVector<TVector<std::string_view>>& values);
    TString GetSql();

private:
    class TImpl;
    const std::unique_ptr<TImpl> Impl;
};

std::unique_ptr<TJsonFilter> NewJsonFilter(
    const TVector<TString>& columns,
    const TVector<TString>& types,
    const TString& whereFilter,
    TJsonFilter::TCallback callback);

} // namespace NFq
