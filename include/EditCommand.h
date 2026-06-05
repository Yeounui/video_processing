#pragma once

#include "ImageBuffer.h"
#include <cstddef>
#include <memory>
#include <QString>

class ProcessingController;

class EditCommand {
public:
    virtual ~EditCommand() = default;

    virtual void undo() = 0;
    virtual void redo() = 0;
    virtual std::size_t memoryBytes() const = 0;
};

class StaticApplyCommand final : public EditCommand {
public:
    StaticApplyCommand(ProcessingController *ctrl,
                       std::shared_ptr<ImageBuffer> prev,
                       std::shared_ptr<ImageBuffer> next,
                       QString label);

    void undo() override;
    void redo() override;
    std::size_t memoryBytes() const override;
    QString label() const;

private:
    ProcessingController *controller_ = nullptr;
    std::shared_ptr<ImageBuffer> prev_;
    std::shared_ptr<ImageBuffer> next_;
    QString label_;
};
